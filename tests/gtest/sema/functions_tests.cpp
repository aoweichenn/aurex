#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, FunctionPrototypes) {
    const fs::path source = positive_sample("functions", "function_prototype.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "item #1 fn add_one prototype",
        "item #2 fn choose prototype",
        "item #4 fn add_one",
        "item #5 fn choose",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn add_one -> i32",
        "fn choose -> i32",
        "fn main -> i32",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn main()",
        "call m0_function_prototype_choose",
        "fn add_one(value: i32)",
        "fn choose(flag: bool, lhs: i32, rhs: i32)",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path mismatch = negative_sample("functions", "function_prototype_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "function prototype and definition signatures do not match");

    const fs::path duplicate = negative_sample("functions", "function_prototype_duplicate.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output, "duplicate function prototype");

    const fs::path missing = negative_sample("functions", "function_prototype_missing_definition.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(missing)).output, "function prototype has no definition");

    const fs::path after_definition = negative_sample("functions", "function_prototype_after_definition.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(after_definition)).output,
        "function prototype must appear before definition"
    );

    const fs::path extern_conflict = negative_sample("functions", "function_extern_definition_conflict.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(extern_conflict)).output,
        "function declaration conflicts with existing function"
    );

    const fs::path param_count = negative_sample("functions", "function_prototype_param_count_mismatch.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(param_count)).output,
        "function prototype and definition signatures do not match"
    );

    const fs::path param_type = negative_sample("functions", "function_prototype_param_type_mismatch.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(param_type)).output,
        "function prototype and definition signatures do not match"
    );

    const fs::path variadic = negative_sample("functions", "function_prototype_variadic_mismatch.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(variadic)).output,
        "function prototype and definition signatures do not match"
    );
}

TEST_F(AurexIntegrationTest, VariadicExternCFunctions) {
    const fs::path source = positive_sample("functions", "variadic_extern_c.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains(ast, "fn snprintf extern_c variadic @name=snprintf");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains(checked, "fn snprintf -> i32 extern_c variadic");

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn snprintf(buffer: *mut u8, size: usize, format: *const u8, ...) @snprintf linkage(extern_c) abi(c)",
        "call snprintf",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path non_extern = negative_sample("functions", "variadic_non_extern.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(non_extern)).output,
        "variadic functions are only supported for extern c declarations"
    );

    const fs::path count = negative_sample("functions", "variadic_argument_count.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(count)).output, "argument count mismatch in call to printf");

    const fs::path not_last = negative_sample("functions", "variadic_not_last.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(not_last)).output, "variadic marker must be last in parameter list");
}

TEST_F(AurexIntegrationTest, DeferScopes) {
    const fs::path source = positive_sample("functions", "defer_scope.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains(ast, "defer");

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn normal_scope(log: *mut u8, index: *mut usize)",
        "fn early_return(log: *mut u8, index: *mut usize)",
        "fn loop_paths(log: *mut u8, index: *mut usize)",
        "fn for_paths(log: *mut u8, index: *mut usize)",
        "call m0_defer_scope_push",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path non_call = negative_sample("functions", "defer_non_call.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(non_call)).output, "defer statement must be a function call");
}

TEST_F(AurexIntegrationTest, ForStatementAndOwnershipSemantics) {
    const fs::path for_source = positive_sample("control_flow", "for_loop.ax");

    const std::string for_ast = require_success(aurexc() + " --emit=ast " + q(for_source)).output;
    expect_contains(for_ast, "for");

    const std::string for_ir = require_success(aurexc() + " --emit=ir " + q(for_source)).output;
    expect_contains_all(for_ir, {
        "for.cond",
        "for.body",
        "for.update",
        "for.exit",
    });
    require_success(aurexc() + " --emit=llvm-ir " + q(for_source));

    const fs::path bad_for_condition = negative_sample("control_flow", "for_condition_bool.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(bad_for_condition)).output, "for condition must be bool");
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("control_flow", "for_init_scope.ax"))).output,
        "unknown name: i"
    );

    const fs::path owner_source = positive_sample("types", "ownership_move.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(owner_source)).output;
    expect_contains_all(checked, {
        "struct Owner noncopy",
        "fn forward -> ownership_move.Owner",
        "fn consume -> i32",
    });

    const std::string owner_ast = require_success(aurexc() + " --emit=ast " + q(owner_source)).output;
    expect_contains(owner_ast, "move_expr");
    require_success(aurexc() + " --emit=llvm-ir " + q(owner_source));

    const fs::path owner_result_source = positive_sample("types", "ownership_result.ax");
    const std::string owner_result_checked = require_success(aurexc() + " --emit=checked " + q(owner_result_source)).output;
    expect_contains_all(owner_result_checked, {
        "struct Owner noncopy",
        "fn check_result_ref_status -> bool",
        "fn check_option_ref_status -> bool",
        "fn wrap -> std.core.result.Result<ownership_result.Owner, i32>",
        "fn unwrap_local_try -> std.core.result.Result<i32, i32>",
    });
    require_success(aurexc() + " --emit=llvm-ir " + q(owner_result_source));

    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_implicit_copy.ax"))).output,
        "non-copyable value must be moved explicitly"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_use_after_move.ax"))).output,
        "use of moved value: first"
    );
    require_success(aurexc() + " --check " + q(positive_sample("types", "move_copyable.ax")));
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "move_non_place.ax"))).output,
        "move requires a local or parameter"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_conditional_move_use.ax"))).output,
        "use of moved value: owner"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_enum_payload_copy.ax"))).output,
        "non-copyable value must be moved explicitly"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_match_without_move.ax"))).output,
        "non-copyable value must be moved explicitly in match value"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_try_without_move.ax"))).output,
        "non-copyable value must be moved explicitly in try expression"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_cstring_copy.ax"))).output,
        "non-copyable value must be moved explicitly"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_bytes_copy.ax"))).output,
        "non-copyable value must be moved explicitly"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_string_copy.ax"))).output,
        "non-copyable value must be moved explicitly"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_path_copy.ax"))).output,
        "non-copyable value must be moved explicitly"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_vec_copy.ax"))).output,
        "non-copyable value must be moved explicitly"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_map_copy.ax"))).output,
        "non-copyable value must be moved explicitly"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_vec_extend_path.ax"))).output,
        "requires copyable element type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_vec_destroy_deep_without_destructor.ax"))).output,
        "requires element type with destructor method"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_vec_clear_deep_without_destructor.ax"))).output,
        "requires element type with destructor method"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_vec_truncate_deep_without_destructor.ax"))).output,
        "requires element type with destructor method"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_map_insert_path_key.ax"))).output,
        "requires copyable key type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_map_get_path_value.ax"))).output,
        "requires copyable value type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_option_is_some.ax"))).output,
        "requires copyable payload type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_result_is_err.ax"))).output,
        "requires copyable ok type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_option_ok_or_error.ax"))).output,
        "requires copyable error type"
    );
    expect_contains(
        require_failure(aurexc() + " --check " + q(negative_sample("types", "noncopy_result_unwrap_or.ax"))).output,
        "requires copyable ok type"
    );
}

TEST_F(AurexIntegrationTest, RecursiveFunctions) {
    const fs::path source = positive_sample("functions", "recursive_functions.ax");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn countdown -> i32",
        "fn even -> bool",
        "fn odd -> bool",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn countdown(value: i32)",
        "call m0_recursive_functions_countdown",
        "fn even(value: i32)",
        "call m0_recursive_functions_odd",
        "fn odd(value: i32)",
        "call m0_recursive_functions_even",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path inferred = negative_sample("inference", "recursive_return_inference.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(inferred)).output,
        "cannot infer recursive function return type without an explicit return type"
    );
}

TEST_F(AurexIntegrationTest, MethodsAndAssociatedFunctions) {
    const fs::path source = positive_sample("functions", "method_calls.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "impl for Counter",
        "fn new for Counter",
        "fn add for Counter",
        "fn read for Counter",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn method method_calls.Counter.new -> method_calls.Counter",
        "fn method method_calls.Counter.add -> i32",
        "fn method method_calls.Counter.read -> i32",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn new(value: i32)",
        "fn add(self: *mut method_calls.Counter, delta: i32)",
        "fn read(self: *const method_calls.Counter)",
        "call m0_method_calls_Counter_new",
        "call m0_method_calls_Counter_add",
        "call m0_method_calls_Counter_read",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path unknown = negative_sample("functions", "unknown_method.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown)).output, "unknown method: unknown_method.Counter.missing");

    const fs::path mutability = negative_sample("functions", "method_receiver_mutability.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mutability)).output, "mutable method receiver requires writable storage");

    const fs::path self_type = negative_sample("functions", "method_self_type.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(self_type)).output, "method self parameter must use the impl type or a pointer to it");

    const fs::path receiver_required = negative_sample("functions", "associated_receiver_required.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(receiver_required)).output, "method requires a receiver");

    const fs::path impl_target = negative_sample("functions", "impl_target_not_named.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(impl_target)).output, "impl target must be a named type");

    const fs::path self_not_first = negative_sample("functions", "method_self_not_first.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(self_not_first)).output, "method self parameter must be first");

    const fs::path receiver_not_place = negative_sample("functions", "method_receiver_not_place.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(receiver_not_place)).output, "method receiver must be a place expression");
}

} // namespace aurex::test
