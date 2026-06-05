#include <support/test_support.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace aurex::test {
namespace {

struct ExpectedDiagnostic {
    std::string_view sample_stem;
    std::string_view message;
};

struct CrossStageNegativeSample {
    std::string_view area;
    std::string_view filename;
    std::string_view diagnostic;
};

struct ImportRuntimeSample {
    std::string_view area;
    std::string_view filename;
};

inline constexpr auto SAMPLE_IMPORT_MARKERS = std::to_array<std::string_view>({
    "import ambiguous.",
    "import bad.",
    "import collide.",
    "import const_mangle_collision.",
    "import cycle.",
    "import lib.",
    "import mangle_collision.",
    "import method_collision.",
    "import qualified_literal.",
    "import samplelib.",
    "import shared.",
});

inline constexpr auto EXPECTED_NEGATIVE_DIAGNOSTICS = std::to_array<ExpectedDiagnostic>({
    {"associated_method_explicit_missing", "unknown method: associated_method_explicit_missing.Box.missing"},
    {"associated_method_explicit_non_generic", "method associated_method_explicit_non_generic.Box.read is not generic"},
    {"associated_method_receiver_required_generic",
        "method requires a receiver: associated_method_receiver_required_generic.Box.id"},
    {"ambiguous_import_alias", "ambiguous import alias: lib"},
    {"ambiguous_import_name", "unknown function: helper"},
    {"array_bool_missing_witness", "match expression over tuple, struct, array, or slice requires an irrefutable arm"},
    {"array_constant_index_out_of_bounds", "array constant index is out of bounds"},
    {"array_literal_empty_infer", "empty array literal requires an array type context"},
    {"array_slice_bound_out_of_bounds", "array constant slice bound is out of bounds"},
    {"array_slice_bounds_order", "array constant slice start exceeds end"},
    {"bare_enum_case_constructor", "unknown function: some"},
    {"bare_enum_case_pattern",
        "bare enum case patterns are not supported; use '.case' or explicit 'Type.case' / 'Type[Args].case'"},
    {"chained_comparison", "comparison operators are non-associative"},
    {"chained_equality", "comparison operators are non-associative"},
    {"const_pattern_enum_value", "enum match pattern must be an enum case or wildcard"},
    {"const_pattern_non_const_name", "unsupported literal match pattern"},
    {"const_pattern_struct_unsupported", "unsupported literal match pattern"},
    {"const_pattern_type_mismatch", "match pattern for integer or bool value must be a literal or wildcard"},
    {"defer_try_argument", "defer statement cannot contain try expression"},
    {"duplicate_type_member_enum_case_method",
        "duplicate type member: duplicate_type_member_enum_case_method.Option.some"},
    {"enum_hash_rejected", "does not satisfy capability `Hash`"},
    {"float_eq_rejected", "type f64 does not satisfy capability `Eq`"},
    {"float_ord_rejected", "type f64 does not satisfy capability `Ord`"},
    {"generic_ptrat_non_pointer", "ptrat target type must be a pointer"},
    {"generic_raw_pointer_method_reference_receiver_rejected", "method receiver type mismatch"},
    {"generic_sizeof_missing_sized", "generic type parameter cannot be queried by sizeof or alignof"},
    {"increment_syntax", "increment operator is not supported"},
    {"enum_payload_bool_missing_witness", "match expression is not exhaustive for enum case"},
    {"import_alias_namespace_conflict",
        "duplicate module member across namespaces in module import_alias_namespace_conflict: util"},
    {"large_array_match_requires_irrefutable",
        "fixed-array match exhaustiveness for arrays longer than 4096 elements requires an irrefutable arm"},
    {"local_shadow_generic_type_parameter", "local name shadows generic type parameter: T"},
    {"local_shadow_import_alias", "local name shadows import alias: util"},
    {"local_shadow_root_module", "local name shadows visible root module: samplelib"},
    {"local_shadow_visible_type", "local name shadows visible type: File"},
    {"match_expression_missing_case", "match expression is not exhaustive for enum case"},
    {"match_guard_literal_false_not_exhaustive", "match expression over integer or bool requires a wildcard arm"},
    {"slice_pattern_non_slice_binding", "slice pattern requires an array or slice value"},
    {"match_open_integer_duplicate", "match arm is unreachable"},
    {"slice_dynamic_length_missing_empty", "match expression over dynamic slice is missing length or element coverage"},
    {"slice_missing_witness", "match expression over dynamic slice is missing length or element coverage"},
    {"structural_match_exhaustiveness_limit",
        "match expression over tuple, struct, array, or slice requires an irrefutable arm"},
    {"tuple_bool_wildcard_tail_missing_witness",
        "match expression over tuple, struct, array, or slice requires an irrefutable arm"},
    {"tuple_open_domain_bool_missing_witness",
        "match expression over tuple, struct, array, or slice requires an irrefutable arm"},
    {"method_abi_collision", "duplicate ABI symbol"},
    {"method_explicit_non_generic", "method method_explicit_non_generic.Box.read is not generic"},
    {"method_local_arg_unify_failure", "cannot infer generic type argument for call to pick"},
    {"method_local_argument_count", "argument count mismatch"},
    {"method_local_explicit_mut_receiver_shared", "mutable method receiver requires mutable pointer"},
    {"method_local_explicit_unknown_type", "unknown type: Missing"},
    {"method_local_inference_failure", "cannot infer generic type argument `T` for call to marker"},
    {"method_local_type_arg_count", "too many generic method type arguments for id: expected 1, got 2"},
    {"method_local_where_unsatisfied", "type f64 does not satisfy capability `Eq`"},
    {"move_indexed_element", "moving an indexed element out of a move-only value is not supported yet"},
    {"move_match_guard_after_move", "use of possibly moved value `current`"},
    {"move_pattern_payload", "consuming pattern payloads are not supported yet"},
    {"move_pattern_condition_payload", "consuming pattern payloads are not supported yet"},
    {"move_try_payload", "try expression transfer of a non-Copy payload is not supported yet"},
    {"move_defer_after_move", "use of moved value `value`"},
    {"move_defer_block_result_after_move", "use of moved value `value`"},
    {"move_defer_exit_after_later_move", "use of moved value `current`"},
    {"move_defer_exit_after_return", "use of moved value `value`"},
    {"move_defer_let_else_after_move", "use of moved value `current`"},
    {"move_defer_try_failure_after_move", "use of moved value `current`"},
    {"move_borrow_alias_after_move", "use of moved value `value`"},
    {"move_borrow_aggregate_alias_after_move", "use of moved value `value`"},
    {"move_borrow_assignment_alias_after_move", "use of moved value `value`"},
    {"move_borrow_block_assignment_alias_after_move", "use of moved value `value`"},
    {"move_borrow_block_alias_after_move", "use of moved value `value`"},
    {"move_borrow_call_alias_after_move", "use of moved value `value`"},
    {"move_defer_borrow_alias_after_move", "use of moved value `value`"},
    {"move_borrow_deref_alias_after_move", "use of moved value `value`"},
    {"move_borrow_enum_alias_after_move", "use of moved value `value`"},
    {"move_borrow_enum_pattern_alias_after_move", "use of moved value `value`"},
    {"move_borrow_field_alias_after_move", "use of moved value `box`"},
    {"move_borrow_generic_method_alias_after_move", "use of moved value `box`"},
    {"move_borrow_if_alias_after_move", "use of moved value `value`"},
    {"move_borrow_inner_block_alias_after_move", "use of moved value `value`"},
    {"move_borrow_index_alias_after_move", "use of moved value `values`"},
    {"move_borrow_match_alias_after_move", "use of moved value `value`"},
    {"move_borrow_method_alias_after_move", "use of moved value `box`"},
    {"move_borrow_name_alias_after_move", "use of moved value `value`"},
    {"move_borrow_or_pattern_alias_after_move", "use of moved value `value`"},
    {"move_borrow_pattern_alias_after_move", "use of moved value `value`"},
    {"move_borrow_reference_field_alias_after_move", "use of moved value `box`"},
    {"move_borrow_slice_source_alias_after_move", "use of moved value `value`"},
    {"move_borrow_slice_pattern_alias_after_move", "use of moved value `value`"},
    {"move_borrow_struct_pattern_literal_alias_after_move", "use of moved value `value`"},
    {"move_borrow_tuple_alias_after_move", "use of moved value `value`"},
    {"move_mut_borrow_alias_after_move", "use of moved value `current`"},
    {"move_return_borrow_alias_after_move", "use of moved value `value`"},
    {"move_use_after_loop", "use of possibly moved value `value`"},
    {"move_use_after_branch", "use of possibly moved value `value`"},
    {"move_use_after_move", "use of moved value `value`"},
    {"module_member_namespace_conflict",
        "duplicate module member across namespaces in module module_member_namespace_conflict: File"},
    {"module_name_mismatch", "does not match import 'bad.name'"},
    {"private_field_access", "field is private: secret"},
    {"private_function_import", "unknown function: add_secret"},
    {"private_qualified_function", "function is private: samplelib.visibility.add_secret"},
    {"reference_assign_through_shared", "left side of assignment must be writable"},
    {"reference_invalid_pointee", "reference requires a valid storage type"},
    {"reference_mut_from_immutable", "mutable reference requires a writable place expression"},
    {"reference_mut_not_raw_pointer", "initializer type does not match declared type"},
    {"reference_return_block_local", "borrowed local storage cannot escape the function"},
    {"reference_return_block_alias_local", "borrowed local storage cannot escape the function"},
    {"reference_mut_return_local", "borrowed local storage cannot escape the function"},
    {"reference_field_assignment_escape", "borrowed local storage cannot escape the function"},
    {"reference_return_deref_alias_local", "borrowed local storage cannot escape the function"},
    {"reference_return_enum_local", "borrowed local storage cannot escape the function"},
    {"reference_return_call_local", "borrowed local storage cannot escape the function"},
    {"reference_return_if_local", "borrowed local storage cannot escape the function"},
    {"reference_return_index_local", "borrowed local storage cannot escape the function"},
    {"reference_return_local", "borrowed local storage cannot escape the function"},
    {"reference_return_match_local", "borrowed local storage cannot escape the function"},
    {"reference_return_method_alias_local", "borrowed local storage cannot escape the function"},
    {"reference_return_method_local", "borrowed local storage cannot escape the function"},
    {"reference_return_param_slot", "borrowed local storage cannot escape the function"},
    {"reference_return_pattern_alias_local", "borrowed local storage cannot escape the function"},
    {"reference_return_slice_pattern_from_local_slice_alias", "borrowed local storage cannot escape the function"},
    {"reference_return_slice_pattern_alias_local", "borrowed local storage cannot escape the function"},
    {"reference_return_struct_local", "borrowed local storage cannot escape the function"},
    {"reference_return_struct_pattern_alias_local", "borrowed local storage cannot escape the function"},
    {"reference_return_tuple_local", "borrowed local storage cannot escape the function"},
    {"raw_pointer_field_requires_unsafe", "raw pointer projection requires unsafe context"},
    {"raw_pointer_field_slice_requires_unsafe", "raw pointer projection requires unsafe context"},
    {"raw_pointer_field_write_requires_unsafe", "raw pointer projection requires unsafe context"},
    {"raw_pointer_index_requires_unsafe", "raw pointer projection requires unsafe context"},
    {"raw_pointer_index_write_requires_unsafe", "raw pointer projection requires unsafe context"},
    {"raw_pointer_method_mut_reference_receiver_rejected", "method receiver type mismatch"},
    {"raw_pointer_method_reference_receiver_rejected", "method receiver type mismatch"},
    {"reference_does_not_satisfy_eq", "type &i32 does not satisfy capability `Eq`"},
    {"reference_hash_rejected", "type &i32 does not satisfy capability `Hash`"},
    {"reference_method_raw_pointer_receiver_rejected", "method receiver type mismatch"},
    {"implicit_address_to_raw_pointer_rejected", "initializer type does not match declared type"},
    {"return_inference_null_non_pointer", "inferred function return types do not match"},
    {"resource_deref_assignment", "resource field, index, or dereference assignment is not supported yet"},
    {"resource_field_assignment", "resource field, index, or dereference assignment is not supported yet"},
    {"resource_index_assignment", "resource field, index, or dereference assignment is not supported yet"},
    {"slice_alias_return_local", "borrowed local storage cannot escape the function"},
    {"slice_return_call_local", "borrowed local storage cannot escape the function"},
    {"slice_return_local", "borrowed local storage cannot escape the function"},
    {"str_slice_bound_non_integer", "slice bound must be an integer"},
    {"str_return_call_local", "borrowed local storage cannot escape the function"},
    {"str_return_local", "borrowed local storage cannot escape the function"},
    {"strraw_return_call_local", "borrowed local storage cannot escape the function"},
    {"strraw_pointer_block_alias_return_local", "borrowed local storage cannot escape the function"},
    {"strraw_pointer_alias_return_local", "borrowed local storage cannot escape the function"},
    {"strraw_pointer_pattern_alias_return_local", "borrowed local storage cannot escape the function"},
    {"strraw_return_strfromutf8_block_pointer_local", "borrowed local storage cannot escape the function"},
    {"strraw_return_strfromutf8_pointer_local", "borrowed local storage cannot escape the function"},
    {"strraw_return_strfromutf8_unsafe_block_pointer_local", "borrowed local storage cannot escape the function"},
    {"strraw_return_slice_call_local", "borrowed local storage cannot escape the function"},
    {"strraw_return_local", "borrowed local storage cannot escape the function"},
    {"strfromutf8_non_slice", "str UTF-8 builtin requires a []const u8 or []mut u8 byte slice"},
    {"try_result_return_mismatch", "try expression on result-like enum requires enclosing function"},
    {"try_result_extra_case", "try expression requires result-like ok/err enum or option-like some/none enum"},
    {"try_option_extra_case", "try expression requires result-like ok/err enum or option-like some/none enum"},
    {"trait_impl_duplicate_method", "duplicate trait impl method"},
    {"trait_impl_missing_method", "trait impl missing method"},
    {"trait_impl_private_trait", "trait is private: samplelib.traits.Hidden"},
    {"trait_impl_self_type_not_named", "impl target must be a named type"},
    {"trait_impl_signature_mismatch", "trait impl method signature does not match requirement"},
    {"trait_impl_target_not_named", "trait impl target must be a named trait"},
    {"trait_impl_unknown_qualified_trait", "unknown trait in module samplelib.traits: Missing"},
    {"trait_impl_unknown_method", "trait impl method is not required"},
    {"trait_impl_unknown_trait", "unknown trait: Missing"},
    {"trait_method_ambiguous_bound", "ambiguous trait method `read`"},
    {"trait_method_ambiguous_impl", "ambiguous trait method `read`"},
    {"trait_method_associated_missing_impl", "has no visible impl for trait method"},
    {"trait_method_missing_bound", "requires a trait bound"},
    {"trait_method_missing_impl", "has no visible impl for trait method"},
    {"trait_associated_type_ambiguous_projection", "ambiguous associated type projection T.Item"},
    {"trait_associated_type_builtin_equality", "builtin capability `Eq` has no associated type `Item`"},
    {"trait_associated_type_duplicate_equality", "duplicate associated type equality for Source.Item"},
    {"trait_associated_type_duplicate_impl",
        "duplicate trait impl associated type: Source for trait_associated_type_duplicate_impl.Bytes.Item"},
    {"trait_associated_type_duplicate_trait", "duplicate trait associated item: Source.Item"},
    {"trait_associated_type_equality_unsatisfied",
        "trait associated type equality is not satisfied: Source for "
        "trait_associated_type_equality_unsatisfied.Bytes.Item expected i32, got u8"},
    {"trait_associated_type_generic_unsupported", "generic associated types are not supported"},
    {"trait_associated_type_missing_bound", "associated type projection T.Item requires a trait bound"},
    {"trait_associated_type_missing_impl",
        "trait impl missing associated type: Source for trait_associated_type_missing_impl.Bytes.Item"},
    {"trait_associated_type_projection_cycle", "associated type equality forms a projection cycle: Source.Item"},
    {"trait_associated_type_signature_mismatch", "trait impl method signature does not match requirement"},
    {"trait_associated_type_unknown_equality", "trait Source has no associated type `Missing`"},
    {"trait_associated_type_unknown_impl",
        "trait impl associated type is not required: Source for trait_associated_type_unknown_impl.Bytes.Other"},
    {"trait_default_method_missing_required",
        "trait impl missing method: Reader for trait_default_method_missing_required.File.read"},
    {"trait_default_method_return_mismatch", "return type mismatch"},
    {"trait_default_method_self_field", "field access requires a non-opaque struct value"},
    {"unknown_module_expr_member", "unknown name in module samplelib.visibility: missing"},
    {"unknown_module_expr_path", "unknown module path: samplelib.missing"},
    {"unknown_module_type_path", "unknown module path: missing.path"},
    {"unsafe_fn_call_required", "call to unsafe function read_raw requires unsafe context"},
});

inline constexpr auto CROSS_STAGE_NEGATIVE_SAMPLES = std::to_array<CrossStageNegativeSample>({
    {"expressions", "increment_syntax.ax", "increment operator is not supported"},
    {"modules", "module_name_mismatch.ax", "does not match import 'bad.name'"},
    {"visibility", "private_qualified_function.ax", "function is private: samplelib.visibility.add_secret"},
    {"types", "array_literal_empty_infer.ax", "empty array literal requires an array type context"},
    {"pattern_matching", "match_expression_missing_case.ax", "match expression is not exhaustive for enum case"},
    {"functions", "unsafe_fn_call_required.ax", "call to unsafe function read_raw requires unsafe context"},
    {"functions", "raw_pointer_method_reference_receiver_rejected.ax", "method receiver type mismatch"},
    {"functions", "defer_try_argument.ax", "defer statement cannot contain try expression"},
    {"types", "reference_return_local.ax", "borrowed local storage cannot escape the function"},
    {"types", "reference_return_call_local.ax", "borrowed local storage cannot escape the function"},
    {"resources", "move_borrow_alias_after_move.ax", "use of moved value `value`"},
    {"resources", "move_borrow_call_alias_after_move.ax", "use of moved value `value`"},
    {"resources", "resource_field_assignment.ax",
        "resource field, index, or dereference assignment is not supported yet"},
    {"error_handling", "try_result_return_mismatch.ax",
        "try expression on result-like enum requires enclosing function"},
});

inline constexpr auto CROSS_STAGE_EMIT_KINDS = std::to_array<driver::EmitKind>({
    driver::EmitKind::check,
    driver::EmitKind::ir,
    driver::EmitKind::llvm_ir,
    driver::EmitKind::executable,
});

inline constexpr auto IMPORT_RUNTIME_SAMPLES = std::to_array<ImportRuntimeSample>({
    {"modules", "full_module_type_path.ax"},
    {"modules", "import_alias_qualified_call.ax"},
    {"modules", "import_path.ax"},
    {"modules", "module_math.ax"},
    {"modules", "module_name_collision.ax"},
    {"types", "type_alias_import.ax"},
    {"visibility", "reexport_import.ax"},
    {"visibility", "visibility_import.ax"},
});

driver::CompilerInvocation sample_invocation(const fs::path& src, const driver::EmitKind emit_kind)
{
    driver::CompilerInvocation invocation;
    invocation.tool_path = aurexc_path();
    invocation.input_path = src;
    invocation.emit_kind = emit_kind;
    return invocation;
}

[[nodiscard]] bool is_native_emit_kind(const driver::EmitKind emit_kind) noexcept
{
    return emit_kind == driver::EmitKind::assembly || emit_kind == driver::EmitKind::object
        || emit_kind == driver::EmitKind::executable;
}

[[nodiscard]] bool sample_uses_import_path(const fs::path& src)
{
    const std::string source = read_text(src);
    for (const std::string_view marker : SAMPLE_IMPORT_MARKERS) {
        if (source.find(marker) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void add_sample_import_path(driver::CompilerInvocation& invocation)
{
    invocation.import_paths.push_back(imports_root());
}

void configure_sample_imports_if_needed(driver::CompilerInvocation& invocation)
{
    if (sample_uses_import_path(invocation.input_path)) {
        add_sample_import_path(invocation);
    }
}

void compile_sample_native(
    const fs::path& src, const fs::path& output, const driver::EmitKind emit_kind, const bool use_sample_imports)
{
    driver::CompilerInvocation invocation = sample_invocation(src, emit_kind);
    invocation.output_path = output;
    if (use_sample_imports) {
        add_sample_import_path(invocation);
    }
    require_compiler_success(invocation);
}

void verify_sample_llvm_ir(const fs::path& src, const bool use_sample_imports)
{
    driver::CompilerInvocation invocation = sample_invocation(src, driver::EmitKind::llvm_ir);
    if (use_sample_imports) {
        add_sample_import_path(invocation);
    }
    require_compiler_success(invocation);
}

void verify_positive_samples_llvm_ir()
{
    for (const fs::path& src : sorted_files(positive_samples_root(), ".ax")) {
        verify_sample_llvm_ir(src, sample_uses_import_path(src));
    }
}

void run_positive_runtime_smoke_sample(const std::string_view area, const std::string_view filename)
{
    const fs::path src = positive_sample(area, filename);
    const fs::path bin = test_bin_root() / stem(src);
    compile_sample_native(src, bin, driver::EmitKind::executable, true);
    require_success(q(bin));
}

void verify_const_enum_lowering()
{
    const std::string const_enum = require_compiler_success(
        sample_invocation(positive_sample("types", "const_enum.ax"), driver::EmitKind::llvm_ir))
                                       .output;
    expect_contains(const_enum, "@m0_const_enum_answer = internal unnamed_addr constant i32 42");
    expect_contains(const_enum, "load i32, ptr @m0_const_enum_answer");
}

void verify_generic_builtin_ir()
{
    const std::string generic_builtins = require_compiler_success(
        sample_invocation(positive_sample("generics", "builtins_m3_1.ax"), driver::EmitKind::ir))
                                             .output;
    expect_contains_all(generic_builtins,
        {
            "sizeof i32",
            "alignof i32",
            "ptraddr",
            "ptrat",
            "ptrcast",
            "bitcast",
            "slice_data",
            "slice_len",
            "strptr",
            "strblen",
            "strvalid",
            "strfromutf8",
            "strraw",
        });
}

[[nodiscard]] std::optional<std::string_view> expected_negative_diagnostic(const fs::path& src)
{
    const std::string name = stem(src);
    for (const ExpectedDiagnostic& expected : EXPECTED_NEGATIVE_DIAGNOSTICS) {
        if (expected.sample_stem == name) {
            return expected.message;
        }
    }
    return std::nullopt;
}

void verify_negative_sample_diagnostics()
{
    for (const fs::path& src : sorted_files(negative_samples_root(), ".ax")) {
        driver::CompilerInvocation invocation = sample_invocation(src, driver::EmitKind::check);
        configure_sample_imports_if_needed(invocation);
        const CommandResult result = require_compiler_failure(invocation);
        expect_contains(result.output, "error:");
        if (const std::optional<std::string_view> expected = expected_negative_diagnostic(src)) {
            expect_contains(result.output, *expected);
        }
    }
}

void verify_negative_sample_fails_in_emit_kind(
    const fs::path& src, const driver::EmitKind emit_kind, const std::string_view diagnostic)
{
    driver::CompilerInvocation invocation = sample_invocation(src, emit_kind);
    configure_sample_imports_if_needed(invocation);
    if (is_native_emit_kind(emit_kind)) {
        invocation.output_path = test_bin_root() / (stem(src) + ".negative");
    }
    const CommandResult result = require_compiler_failure(invocation);
    expect_contains(result.output, diagnostic);
}

void verify_cross_stage_negative_samples()
{
    for (const CrossStageNegativeSample& sample : CROSS_STAGE_NEGATIVE_SAMPLES) {
        const fs::path src = negative_sample(sample.area, sample.filename);
        for (const driver::EmitKind emit_kind : CROSS_STAGE_EMIT_KINDS) {
            verify_negative_sample_fails_in_emit_kind(src, emit_kind, sample.diagnostic);
        }
    }
}

void verify_import_runtime_sample(const ImportRuntimeSample& sample)
{
    const fs::path src = positive_sample(sample.area, sample.filename);

    driver::CompilerInvocation modules = sample_invocation(src, driver::EmitKind::modules);
    add_sample_import_path(modules);
    const std::string module_dump = require_compiler_success(modules).output;
    expect_contains(module_dump, stem(src));

    driver::CompilerInvocation checked = sample_invocation(src, driver::EmitKind::check);
    add_sample_import_path(checked);
    require_compiler_success(checked);

    driver::CompilerInvocation ir = sample_invocation(src, driver::EmitKind::ir);
    add_sample_import_path(ir);
    expect_contains(require_compiler_success(ir).output, "aurex_ir v0");

    driver::CompilerInvocation llvm_ir = sample_invocation(src, driver::EmitKind::llvm_ir);
    add_sample_import_path(llvm_ir);
    expect_contains(require_compiler_success(llvm_ir).output, "define i32 @main");

    const fs::path bin = test_bin_root() / stem(src);
    compile_sample_native(src, bin, driver::EmitKind::executable, true);
    EXPECT_EQ(require_success(q(bin)).output, "");
}

void verify_import_runtime_samples()
{
    for (const ImportRuntimeSample& sample : IMPORT_RUNTIME_SAMPLES) {
        verify_import_runtime_sample(sample);
    }
}

} // namespace

TEST_F(AurexIntegrationTest, SampleSuite_PositiveSamples)
{
    verify_positive_samples_llvm_ir();
    verify_const_enum_lowering();
    verify_generic_builtin_ir();
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_pointer_field_write)
{
    run_positive_runtime_smoke_sample("pointers", "pointer_field_write.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_eval_order_assign)
{
    run_positive_runtime_smoke_sample("evaluation", "eval_order_assign.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_const_binary)
{
    run_positive_runtime_smoke_sample("types", "const_binary.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_builtins)
{
    run_positive_runtime_smoke_sample("core", "builtins.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_compound_assignment)
{
    run_positive_runtime_smoke_sample("expressions", "compound_assignment.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_block_expression)
{
    run_positive_runtime_smoke_sample("expressions", "block_expression.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_tuple_basic)
{
    run_positive_runtime_smoke_sample("types", "tuple_basic.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_str_checked)
{
    run_positive_runtime_smoke_sample("types", "str_checked.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_str_slice)
{
    run_positive_runtime_smoke_sample("types", "str_slice.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_pattern_ergonomics)
{
    run_positive_runtime_smoke_sample("pattern_matching", "pattern_ergonomics.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_pattern_remaining)
{
    run_positive_runtime_smoke_sample("pattern_matching", "pattern_remaining.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_const_pattern)
{
    run_positive_runtime_smoke_sample("pattern_matching", "const_pattern.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_generic_basic)
{
    run_positive_runtime_smoke_sample("generics", "basic_m2.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_generic_builtins)
{
    run_positive_runtime_smoke_sample("generics", "builtins_m3_1.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_method_local_generics)
{
    run_positive_runtime_smoke_sample("generics", "method_local_m3_1.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_method_local_identity_closure_generics)
{
    run_positive_runtime_smoke_sample("generics", "method_local_identity_closure_m3_1.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_method_static_dispatch)
{
    run_positive_runtime_smoke_sample("traits", "trait_method_static_dispatch.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_method_associated_static_dispatch)
{
    run_positive_runtime_smoke_sample("traits", "trait_method_associated_static_dispatch.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_associated_type_where_equality)
{
    run_positive_runtime_smoke_sample("traits", "trait_associated_type_where_equality.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_method_inherent_precedence)
{
    run_positive_runtime_smoke_sample("traits", "trait_method_inherent_precedence.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_method_function_field_precedence)
{
    run_positive_runtime_smoke_sample("traits", "trait_method_function_field_precedence.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_imported_samples)
{
    verify_import_runtime_samples();
}

TEST_F(AurexIntegrationTest, SampleSuite_NegativeSamples)
{
    verify_negative_sample_diagnostics();
}

TEST_F(AurexIntegrationTest, SampleSuite_NegativeCrossStageSamples)
{
    verify_cross_stage_negative_samples();
}

} // namespace aurex::test
