#include <aurex/frontend/sema/sema_messages.hpp>

#include <support/test_support.hpp>

#include <fstream>

namespace aurex::test {
namespace {

[[nodiscard]] fs::path write_m7b_regression_source(const std::string_view name, const std::string_view source)
{
    const fs::path dir = tmp_root() / "m7b-regressions";
    fs::create_directories(dir);
    const fs::path path = dir / (std::string(name) + ".ax");
    std::ofstream out(path);
    out << source;
    return path;
}

void expect_m7b_check_success(const std::string_view name, const std::string_view source)
{
    const fs::path path = write_m7b_regression_source(name, source);
    require_success(aurexc() + " --check " + q(path));
}

void expect_m7b_check_failure(
    const std::string_view name, const std::string_view source, const std::string_view diagnostic)
{
    const fs::path path = write_m7b_regression_source(name, source);
    expect_contains(require_failure(aurexc() + " --check " + q(path)).output, diagnostic);
}

} // namespace

TEST_F(AurexIntegrationTest, FunctionPrototypes)
{
    const fs::path source = positive_sample("functions", "function_prototype.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast,
        {
            "item #1 priv fn add_one prototype",
            "item #2 priv fn choose prototype",
            "item #4 priv fn add_one",
            "item #5 priv fn choose",
        });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "fn priv add_one -> i32",
            "fn priv choose -> i32",
            "fn priv main -> i32",
        });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "fn main()",
            "call m0_function_prototype_choose",
            "fn add_one(value: i32)",
            "fn choose(flag: bool, lhs: i32, rhs: i32)",
        });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path mismatch = negative_sample("functions", "function_prototype_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output,
        "function prototype and definition signatures do not match");

    const fs::path duplicate = negative_sample("functions", "function_prototype_duplicate.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output, "duplicate function prototype");

    const fs::path missing = negative_sample("functions", "function_prototype_missing_definition.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(missing)).output, "function prototype has no definition");

    const fs::path after_definition = negative_sample("functions", "function_prototype_after_definition.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(after_definition)).output,
        "function prototype must appear before definition");

    const fs::path extern_conflict = negative_sample("functions", "function_extern_definition_conflict.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(extern_conflict)).output,
        "function declaration conflicts with existing function");

    const fs::path param_count = negative_sample("functions", "function_prototype_param_count_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(param_count)).output,
        "function prototype and definition signatures do not match");

    const fs::path param_type = negative_sample("functions", "function_prototype_param_type_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(param_type)).output,
        "function prototype and definition signatures do not match");

    const fs::path variadic = negative_sample("functions", "function_prototype_variadic_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(variadic)).output,
        "function prototype and definition signatures do not match");
}

TEST_F(AurexIntegrationTest, MultiParameterFunctionAcceptsCIdentifier)
{
    const fs::path source = positive_sample("functions", "multi_param_c_identifier.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast,
        {
            "priv fn mul_add",
            "param c : i32",
            "let c : i32",
        });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains(ir, "fn mul_add(a: i32, b: i32, c: i32)");

    const fs::path bin = test_bin_root() / "multi_param_c_identifier";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    require_success(q(bin));
}

TEST_F(AurexIntegrationTest, VariadicExternCFunctions)
{
    const fs::path source = positive_sample("functions", "variadic_extern_c.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains(ast, "fn snprintf extern_c variadic @name=snprintf");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains(checked, "fn priv snprintf -> i32 extern_c variadic");

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "fn snprintf(buffer: *mut u8, size: usize, format: *const u8, ...) @snprintf linkage(extern_c) abi(c)",
            "call snprintf",
        });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path non_extern = negative_sample("functions", "variadic_non_extern.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(non_extern)).output,
        "variadic functions are only supported for extern c declarations");

    const fs::path count = negative_sample("functions", "variadic_argument_count.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(count)).output, "argument count mismatch in call to printf");

    const fs::path invalid_infer = negative_sample("functions", "variadic_argument_infer_invalid.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(invalid_infer)).output,
        "variadic argument type cannot be inferred in call to printf");

    const fs::path not_last = negative_sample("functions", "variadic_not_last.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(not_last)).output, "variadic marker must be last in parameter list");
}

TEST_F(AurexIntegrationTest, AbiCollisionDiagnosticsIncludePreviousDeclaration)
{
    const fs::path extern_mismatch = negative_sample("functions", "extern_abi_signature_mismatch.ax");
    expect_contains_all(require_failure(aurexc() + " --check " + q(extern_mismatch)).output,
        {
            "extern C ABI symbol redeclared with incompatible signature: same_extern_symbol",
            "note: previous declaration of `same_extern_symbol` is here",
        });

    const fs::path method_collision = negative_sample("functions", "method_abi_collision.ax");
    expect_contains_all(
        require_failure(aurexc() + " --check " + sample_import_flags() + " " + q(method_collision)).output,
        {
            "duplicate ABI symbol",
            "note: previous declaration",
        });
}

TEST_F(AurexIntegrationTest, FunctionTypesAndIndirectCalls)
{
    const fs::path basic = positive_sample("functions", "function_type_basic.ax");
    const std::string basic_checked = require_success(aurexc() + " --emit=checked " + q(basic)).output;
    expect_contains(basic_checked, "type priv BinaryOp = fn(i32, i32) -> i32");
    const std::string basic_ir = require_success(aurexc() + " --emit=ir " + q(basic)).output;
    expect_contains_all(basic_ir, {"function_ref @m0_function_type_basic_add", "call %"});
    const fs::path basic_bin = test_bin_root() / "function_type_basic";
    require_success(aurexc() + " " + q(basic) + " -o " + q(basic_bin));
    require_success(q(basic_bin));

    const fs::path field = positive_sample("functions", "function_type_struct_field.ax");
    const std::string field_ir = require_success(aurexc() + " --emit=ir " + q(field)).output;
    expect_contains_all(
        field_ir, {".add: fn(i32, i32) -> i32", "function_ref @m0_function_type_struct_field_add", "call %"});
    const fs::path field_bin = test_bin_root() / "function_type_struct_field";
    require_success(aurexc() + " " + q(field) + " -o " + q(field_bin));
    require_success(q(field_bin));

    const fs::path returned = positive_sample("functions", "function_type_return.ax");
    const std::string returned_ir = require_success(aurexc() + " --emit=ir " + q(returned)).output;
    expect_contains_all(
        returned_ir, {"fn choose()", " -> fn(i32) -> i32", "function_ref @m0_function_type_return_inc", "call %"});
    const fs::path returned_bin = test_bin_root() / "function_type_return";
    require_success(aurexc() + " " + q(returned) + " -o " + q(returned_bin));
    require_success(q(returned_bin));

    const fs::path lambda = positive_sample("functions", "lambda_function_literal.ax");
    const std::string lambda_ast = require_success(aurexc() + " --emit=ast " + q(lambda)).output;
    expect_contains_all(lambda_ast,
        {
            "lambda [](value: i32) -> i32",
            "return",
            "binary",
        });
    const std::string lambda_checked = require_success(aurexc() + " --emit=checked " + q(lambda)).output;
    expect_contains(lambda_checked, "fn priv apply -> i32");
    const std::string lambda_ir = require_success(aurexc() + " --emit=ir " + q(lambda)).output;
    expect_contains_all(lambda_ir, {"fn __aurex_lambda", "function_ref @__aurex_lambda", "call %"});
    const fs::path lambda_bin = test_bin_root() / "lambda_function_literal";
    require_success(aurexc() + " " + q(lambda) + " -o " + q(lambda_bin));
    require_success(q(lambda_bin));

    const fs::path closure = positive_sample("functions", "lambda_closure_capture.ax");
    const std::string closure_checked = require_success(aurexc() + " --emit=checked " + q(closure)).output;
    expect_contains_all(closure_checked, {"__aurex_lambda", "__aurex_lambda_m", "_env"});
    const std::string closure_ir = require_success(aurexc() + " --emit=ir " + q(closure)).output;
    expect_contains_all(
        closure_ir, {"record __aurex_lambda", "_env", "fn __aurex_lambda", "closure.env", "call __aurex_lambda"});
    const fs::path closure_bin = test_bin_root() / "lambda_closure_capture";
    require_success(aurexc() + " " + q(closure) + " -o " + q(closure_bin));
    require_success(q(closure_bin));

    const fs::path returned_closure = positive_sample("functions", "lambda_closure_return.ax");
    const std::string returned_closure_checked =
        require_success(aurexc() + " --emit=checked " + q(returned_closure)).output;
    expect_contains_all(returned_closure_checked, {"fn priv make_adder -> __aurex_lambda", "_env"});
    const std::string returned_closure_ir = require_success(aurexc() + " --emit=ir " + q(returned_closure)).output;
    expect_contains_all(returned_closure_ir,
        {"fn make_adder(base: i32)", "-> __aurex_lambda", "aggregate {.__capture_0_base", "call __aurex_lambda"});
    const fs::path returned_closure_bin = test_bin_root() / "lambda_closure_return";
    require_success(aurexc() + " " + q(returned_closure) + " -o " + q(returned_closure_bin));
    require_success(q(returned_closure_bin));

    const fs::path extern_c = positive_sample("functions", "function_type_extern_c.ax");
    const std::string extern_checked = require_success(aurexc() + " --emit=checked " + q(extern_c)).output;
    expect_contains_all(extern_checked,
        {"type priv Strlen = extern c fn(*const u8) -> usize",
            "type priv Snprintf = extern c fn(*mut u8, usize, *const u8, ...) -> i32"});
    require_success(aurexc() + " --emit=llvm-ir " + q(extern_c));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "function_type_arg_mismatch.ax")))
            .output,
        "argument type mismatch in call to op");
    expect_contains(
        require_failure(aurexc() + " --check "
                        + q(negative_sample("functions", "lambda_capture_function_pointer_mismatch.ax")))
            .output,
        "initializer type does not match declared type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "lambda_capture_non_copy.ax"))).output,
        "capturing a non-Copy value in a closure is not supported yet");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "lambda_capture_borrowed_view.ax")))
            .output,
        "capturing a borrowed-view value in a closure is not supported yet");
    expect_contains(require_failure(aurexc() + " --check "
                                    + q(negative_sample("functions", "lambda_capture_generic_dependent.ax")))
                        .output,
        "capturing a generic-dependent value in a closure is not supported yet");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "lambda_capture_assign.ax"))).output,
        "left side of assignment must be writable");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "lambda_reference_capture.ax")))
            .output,
        "reference capture in closures is not supported yet");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "lambda_missing_return.ax"))).output,
        "not all control paths return a value");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "function_type_return_mismatch.ax")))
            .output,
        "initializer type does not match declared type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "function_type_callconv_mismatch.ax")))
            .output,
        "initializer type does not match declared type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "function_type_unsafe_mismatch.ax")))
            .output,
        "initializer type does not match declared type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "function_type_array_param.ax")))
            .output,
        "array type cannot be used as a function type parameter");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("functions", "function_type_array_return.ax")))
            .output,
        "array type cannot be used as a function type return");
    expect_contains(require_failure(aurexc() + " --check "
                        + q(negative_sample("functions", "function_type_variadic_non_extern.ax")))
                        .output,
        "variadic function types are only supported for extern c fn");
}

TEST_F(AurexIntegrationTest, PublicFunctionsRequireExplicitReturnType)
{
    const fs::path source = negative_sample("functions", "pub_inferred_return.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(source)).output, "public function return type must be explicit");
}

TEST_F(AurexIntegrationTest, DeferScopes)
{
    const fs::path source = positive_sample("functions", "defer_scope.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains(ast, "defer");

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "fn normal_scope(log: *mut u8, index: *mut usize)",
            "fn early_return(log: *mut u8, index: *mut usize)",
            "fn loop_paths(log: *mut u8, index: *mut usize)",
            "fn for_paths(log: *mut u8, index: *mut usize)",
            "call m0_defer_scope_push",
        });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path non_call = negative_sample("functions", "defer_non_call.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(non_call)).output, "defer statement must be a function call");
}

TEST_F(AurexIntegrationTest, ForStatementAndValueSemantics)
{
    const fs::path for_source = positive_sample("control_flow", "for_loop.ax");

    const std::string for_ast = require_success(aurexc() + " --emit=ast " + q(for_source)).output;
    expect_contains_all(for_ast,
        {
            "for",
            "for_range i",
            "for_range k",
        });

    const std::string for_ir = require_success(aurexc() + " --emit=ir " + q(for_source)).output;
    expect_contains_all(for_ir,
        {
            "for.cond",
            "for.body",
            "for.update",
            "for.exit",
            "for.range.cond",
            "for.range.body",
            "for.range.update",
            "for.range.exit",
        });
    require_success(aurexc() + " --emit=llvm-ir " + q(for_source));

    const fs::path range_source = positive_sample("control_flow", "for_range.ax");
    const std::string range_ir = require_success(aurexc() + " --emit=ir " + q(range_source)).output;
    expect_contains_all(range_ir,
        {
            "fn range_control_paths()",
            "fn range_step_paths()",
            "for.range.step",
            "for.range.cond",
            "for.range.body",
            "for.range.update",
            "for.range.exit",
        });
    const fs::path range_bin = test_bin_root() / "for_range";
    require_success(aurexc() + " " + q(range_source) + " -o " + q(range_bin));
    require_success(q(range_bin));

    const fs::path bad_for_condition = negative_sample("control_flow", "for_condition_bool.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(bad_for_condition)).output, "for condition must be bool");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_init_scope.ax"))).output,
        "unknown name: i");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_range_non_integer.ax"))).output,
        "range bounds must be integer");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_range_start_non_integer.ax")))
            .output,
        "range bounds must be integer");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_range_type_mismatch.ax")))
            .output,
        "range bounds must have the same type");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_range_scope.ax"))).output,
        "unknown name: i");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_range_arity.ax"))).output,
        "range expects 1 to 3 arguments");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_range_step_non_integer.ax")))
            .output,
        "range step must be integer");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_range_step_type_mismatch.ax")))
            .output,
        "range step must have the same type as bounds");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_range_too_many_args.ax")))
            .output,
        "range expects 1 to 3 arguments");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_in_unsupported.ax"))).output,
        "M2 range-for only supports range(...); generic iteration is not part of M2 syntax");

    const fs::path value_source = positive_sample("types", "value_flow.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(value_source)).output;
    expect_contains_all(checked,
        {
            "struct priv Owner",
            "fn priv forward -> value_flow.Owner",
            "fn priv consume -> i32",
        });
    require_success(aurexc() + " --emit=llvm-ir " + q(value_source));

    const fs::path result_value_source = positive_sample("types", "result_value_flow.ax");
    const std::string result_value_checked =
        require_success(aurexc() + " --emit=checked " + q(result_value_source)).output;
    expect_contains_all(result_value_checked,
        {
            "struct priv Owner",
            "fn priv check_result_ref_status -> bool",
            "fn priv check_option_ref_status -> bool",
            "fn priv wrap -> result_value_flow.ResultOwnerI32",
            "fn priv unwrap_local_try -> result_value_flow.ResultI32I32",
        });
    require_success(aurexc() + " --emit=llvm-ir " + q(result_value_source));
    require_success(aurexc() + " --check " + q(positive_sample("types", "plain_value.ax")));
}

TEST_F(AurexIntegrationTest, RecursiveFunctions)
{
    const fs::path source = positive_sample("functions", "recursive_functions.ax");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "fn priv countdown -> i32",
            "fn priv even -> bool",
            "fn priv odd -> bool",
        });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "fn countdown(value: i32)",
            "call m0_recursive_functions_countdown",
            "fn even(value: i32)",
            "call m0_recursive_functions_odd",
            "fn odd(value: i32)",
            "call m0_recursive_functions_even",
        });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path inferred = negative_sample("inference", "recursive_return_inference.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(inferred)).output,
        "cannot infer recursive function return type without an explicit return type");
}

TEST_F(AurexIntegrationTest, MethodsAndAssociatedFunctions)
{
    const fs::path source = positive_sample("functions", "method_calls.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast,
        {
            "impl for Counter",
            "fn new for Counter",
            "fn add for Counter",
            "fn read for Counter",
        });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "fn method method_calls.Counter.new -> method_calls.Counter",
            "fn method method_calls.Counter.add -> i32",
            "fn method method_calls.Counter.read -> i32",
            "receiver_access=mutable auto_borrow=true two_phase=true",
            "receiver_access=shared auto_borrow=true two_phase=false",
        });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "fn new(value: i32)",
            "fn add(self: &mut method_calls.Counter, delta: i32)",
            "fn read(self: &method_calls.Counter)",
            "call m0_method_calls_Counter_new",
            "call m0_method_calls_Counter_add",
            "call m0_method_calls_Counter_read",
        });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path two_phase = positive_sample("functions", "method_two_phase_receiver.ax");
    const std::string two_phase_checked = require_success(aurexc() + " --emit=checked " + q(two_phase)).output;
    expect_contains_all(two_phase_checked,
        {
            "receiver_access=mutable auto_borrow=true two_phase=true",
            "receiver_access=shared auto_borrow=true two_phase=false",
        });
    require_success(aurexc() + " --check " + q(two_phase));

    const fs::path preborrowed_receiver = positive_sample("types", "reference_basic.ax");
    const std::string preborrowed_checked =
        require_success(aurexc() + " --emit=checked " + q(preborrowed_receiver)).output;
    expect_contains_all(preborrowed_checked,
        {
            "receiver_access=mutable auto_borrow=false two_phase=false",
            "receiver_access=shared auto_borrow=false two_phase=false",
        });

    const fs::path nested_two_phase = negative_sample("functions", "method_two_phase_nested_mut_receiver.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(nested_two_phase)).output, sema::SEMA_TWO_PHASE_RECEIVER_CONFLICT);

    const fs::path unknown = negative_sample("functions", "unknown_method.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(unknown)).output, "unknown method: unknown_method.Counter.missing");

    const fs::path mutability = negative_sample("functions", "method_receiver_mutability.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mutability)).output,
        "mutable method receiver requires writable storage");

    const fs::path self_type = negative_sample("functions", "method_self_type.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(self_type)).output,
        "method self parameter must use the impl type or a pointer to it");

    const fs::path receiver_required = negative_sample("functions", "associated_receiver_required.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(receiver_required)).output, "method requires a receiver");

    const fs::path impl_target = negative_sample("functions", "impl_target_not_named.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(impl_target)).output, "impl target must be a named type");

    const fs::path self_not_first = negative_sample("functions", "method_self_not_first.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(self_not_first)).output, "method self parameter must be first");

    const fs::path receiver_not_place = negative_sample("functions", "method_receiver_not_place.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(receiver_not_place)).output,
        "method receiver must be a place expression");
}

TEST_F(AurexIntegrationTest, M7bBorrowContractsUseDeclaredCalleeBoundaries)
{
    expect_m7b_check_success("extern_contract_call",
        "module m7b.extern_contract_call;\n"
        "extern c {\n"
        "  @borrow(return = [value])\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "@borrow(return = [value])\n"
        "fn wrap(value: &i32) -> &i32 { return ext(value); }\n"
        "fn main() -> void {}\n");

    expect_m7b_check_success("forward_declared_contract",
        "module m7b.forward_declared_contract;\n"
        "@borrow(return = [value])\n"
        "fn wrap(value: &i32) -> &i32 { return id(value); }\n"
        "@borrow(return = [value])\n"
        "fn id(value: &i32) -> &i32 { return value; }\n"
        "fn main() -> void {}\n");

    expect_m7b_check_failure("branch_assignment_contract_too_narrow",
        "module m7b.branch_assignment_contract_too_narrow;\n"
        "@borrow(return = [right])\n"
        "fn choose(left: &i32, right: &i32, take_left: bool) -> &i32 {\n"
        "  var result: &i32 = left;\n"
        "  if !take_left { result = right; }\n"
        "  return result;\n"
        "}\n"
        "fn main() -> void {}\n",
        sema::SEMA_BORROW_CONTRACT_MISMATCH);
}

TEST_F(AurexIntegrationTest, M7bReturnedBorrowViewsParticipateInLocalLoanChecking)
{
    expect_m7b_check_failure("call_return_local_alias",
        "module m7b.call_return_local_alias;\n"
        "@borrow(return = [value])\n"
        "fn id(value: &i32) -> &i32 { return value; }\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = id(&value);\n"
        "  value = 2;\n"
        "  let _: i32 = *ref;\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);

    expect_m7b_check_failure("slice_alias_write",
        "module m7b.slice_alias_write;\n"
        "fn main() -> void {\n"
        "  var values: [3]i32 = [1, 2, 3];\n"
        "  let slice: []i32 = values[:];\n"
        "  values[0] = 9;\n"
        "  let _: i32 = slice[0];\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);

    expect_m7b_check_failure("strfromutf8_alias_write",
        "module m7b.strfromutf8_alias_write;\n"
        "fn main() -> void {\n"
        "  var bytes: [2]u8 = [65, 66];\n"
        "  let text: str = strfromutf8(bytes[:]);\n"
        "  bytes[0] = 67;\n"
        "  let _: usize = text.len;\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);

    expect_m7b_check_failure("extern_unknown_return_alias_write",
        "module m7b.extern_unknown_return_alias_write;\n"
        "extern c {\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = ext(&value);\n"
        "  value = 2;\n"
        "  let _: i32 = *ref;\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);

    expect_m7b_check_failure("declared_unknown_return_alias_write",
        "module m7b.declared_unknown_return_alias_write;\n"
        "extern c {\n"
        "  @borrow(return = [unknown])\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = ext(&value);\n"
        "  value = 2;\n"
        "  let _: i32 = *ref;\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);

    expect_m7b_check_success("unknown_return_allows_unrelated_local_write",
        "module m7b.unknown_return_allows_unrelated_local_write;\n"
        "extern c {\n"
        "  @borrow(return = [unknown])\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = ext(&value);\n"
        "  var other: i32 = 2;\n"
        "  other = 3;\n"
        "  let _: i32 = *ref;\n"
        "}\n");

    expect_m7b_check_failure("inferred_unknown_wrapper_alias_write",
        "module m7b.inferred_unknown_wrapper_alias_write;\n"
        "extern c {\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "fn wrap(value: &i32) -> &i32 { return ext(value); }\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = wrap(&value);\n"
        "  value = 2;\n"
        "  let _: i32 = *ref;\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);

    expect_m7b_check_failure("declared_param_wrapper_alias_write",
        "module m7b.declared_param_wrapper_alias_write;\n"
        "extern c {\n"
        "  @borrow(return = [value])\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "fn wrap(value: &i32) -> &i32 { return ext(value); }\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = wrap(&value);\n"
        "  value = 2;\n"
        "  let _: i32 = *ref;\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);

    expect_m7b_check_failure("declared_unknown_wrapper_alias_write",
        "module m7b.declared_unknown_wrapper_alias_write;\n"
        "extern c {\n"
        "  @borrow(return = [unknown])\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "fn wrap(value: &i32) -> &i32 { return ext(value); }\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = wrap(&value);\n"
        "  value = 2;\n"
        "  let _: i32 = *ref;\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);

    expect_m7b_check_success("aggregate_unknown_tag_write_allows_source_use",
        "module m7b.aggregate_unknown_tag_write_allows_source_use;\n"
        "struct Holder { borrowed: &i32; tag: i32; }\n"
        "extern c {\n"
        "  @borrow(return = [unknown])\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  var holder: Holder = Holder { borrowed: ext(&value), tag: 1 };\n"
        "  holder.tag = 2;\n"
        "  let _: i32 = *holder.borrowed;\n"
        "}\n");

    expect_m7b_check_success("declared_static_return_allows_source_write",
        "module m7b.declared_static_return_allows_source_write;\n"
        "extern c {\n"
        "  @borrow(return = [static])\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = ext(&value);\n"
        "  value = 2;\n"
        "  let _: i32 = *ref;\n"
        "}\n");

    expect_m7b_check_success("declared_static_wrapper_allows_source_write",
        "module m7b.declared_static_wrapper_allows_source_write;\n"
        "extern c {\n"
        "  @borrow(return = [static])\n"
        "  fn ext(value: &i32) -> &i32;\n"
        "}\n"
        "fn wrap(value: &i32) -> &i32 { return ext(value); }\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let ref: &i32 = wrap(&value);\n"
        "  value = 2;\n"
        "  let _: i32 = *ref;\n"
        "}\n");

    expect_m7b_check_failure("aggregate_return_alias_write",
        "module m7b.aggregate_return_alias_write;\n"
        "struct Holder { borrowed: &i32; tag: i32; }\n"
        "@borrow(return = [value])\n"
        "fn id(value: &i32) -> &i32 { return value; }\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let holder: Holder = Holder { borrowed: id(&value), tag: 1 };\n"
        "  value = 2;\n"
        "  let _: i32 = *holder.borrowed;\n"
        "}\n",
        sema::SEMA_ACTIVE_BORROW_CONFLICT);

    expect_m7b_check_success("aggregate_unread_borrow_allows_source_write",
        "module m7b.aggregate_unread_borrow_allows_source_write;\n"
        "struct Holder { borrowed: &i32; tag: i32; }\n"
        "@borrow(return = [value])\n"
        "fn id(value: &i32) -> &i32 { return value; }\n"
        "fn main() -> void {\n"
        "  var value: i32 = 1;\n"
        "  let holder: Holder = Holder { borrowed: id(&value), tag: 1 };\n"
        "  value = 2;\n"
        "  let _: i32 = holder.tag;\n"
        "}\n");
}

TEST_F(AurexIntegrationTest, M7dBorrowEscapeFallbackCoversPatternStorageGuard)
{
    expect_m7b_check_success("pattern_storage_guard_success",
        "module m7d.pattern_storage_guard_success;\n"
        "struct Holder {\n"
        "  value: i32;\n"
        "}\n"
        "struct Pair {\n"
        "  left: i32;\n"
        "  right: i32;\n"
        "}\n"
        "struct RefPair {\n"
        "  left: &i32;\n"
        "  right: &i32;\n"
        "}\n"
        "impl RefPair {\n"
        "  @borrow(return = [self])\n"
        "  fn left_ref(self: &RefPair) -> &i32 {\n"
        "    return self.left;\n"
        "  }\n"
        "}\n"
        "enum MaybeValue {\n"
        "  some(i32),\n"
        "  other(i32),\n"
        "  none,\n"
        "}\n"
        "enum MaybeRef {\n"
        "  some(&i32),\n"
        "  none,\n"
        "}\n"
        "@borrow(return = [value])\n"
        "fn choose_ref(value: &i32) -> &i32 {\n"
        "  return value;\n"
        "}\n"
        "fn store(slot: &mut Holder, take_left: bool) -> void {\n"
        "  let local: i32 = 10;\n"
        "  let borrowed: &i32 = &local;\n"
        "  let tuple_refs: (&i32, &i32) = (&local, borrowed);\n"
        "  let struct_refs: RefPair = RefPair { left: &local, right: borrowed };\n"
        "  let selected: &i32 = if take_left {\n"
        "    &local\n"
        "  } else {\n"
        "    borrowed\n"
        "  };\n"
        "  let matched: &i32 = match MaybeRef.some(&local) {\n"
        "    .some(value) => value,\n"
        "    .none => borrowed,\n"
        "  };\n"
        "  let block_ref: &i32 = {\n"
        "    let inside: &i32 = choose_ref(borrowed);\n"
        "    inside\n"
        "  };\n"
        "  let method_ref: &i32 = struct_refs.left_ref();\n"
        "  if *method_ref == *selected {\n"
        "    slot.value = *matched;\n"
        "  }\n"
        "  if *block_ref == *borrowed {\n"
        "    slot.value = *block_ref;\n"
        "  }\n"
        "  let (tuple_value,) = (1,);\n"
        "  slot.value = tuple_value;\n"
        "  let [array_value] = [2];\n"
        "  slot.value = array_value;\n"
        "  let Pair { left: struct_value, right: _ } = Pair { left: 3, right: 4 };\n"
        "  slot.value = struct_value;\n"
        "  let maybe: MaybeValue = if take_left {\n"
        "    MaybeValue.some(5)\n"
        "  } else {\n"
        "    MaybeValue.other(6)\n"
        "  };\n"
        "  let .some(enum_value) | .other(enum_value) = maybe else {\n"
        "    return;\n"
        "  };\n"
        "  slot.value = enum_value;\n"
        "}\n"
        "fn main() -> void {}\n");
}

TEST_F(AurexIntegrationTest, M7bBlockExpressionPrecheckScansInternalStatements)
{
    expect_m7b_check_failure("block_expr_precheck",
        "module m7b.block_expr_precheck;\n"
        "struct Bag { value: i32 }\n"
        "impl Bag {\n"
        "  fn len(self: &Bag) -> i32 { return self.value; }\n"
        "  fn set(self: &mut Bag, value: i32) -> void { self.value = value; }\n"
        "}\n"
        "fn main() -> void {\n"
        "  var bag: Bag = Bag { value: 1 };\n"
        "  let _: i32 = {\n"
        "    bag.set({ bag.value = 2; 3 });\n"
        "    0\n"
        "  };\n"
        "}\n",
        sema::SEMA_TWO_PHASE_RECEIVER_CONFLICT);
}

} // namespace aurex::test
