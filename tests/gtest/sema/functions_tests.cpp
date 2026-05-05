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

    const fs::path bin = test_bin_root() / "function_prototype";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path mismatch = negative_sample("functions", "function_prototype_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "function prototype and definition signatures do not match");

    const fs::path duplicate = negative_sample("functions", "function_prototype_duplicate.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output, "duplicate function prototype");

    const fs::path missing = negative_sample("functions", "function_prototype_missing_definition.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(missing)).output, "function prototype has no definition");
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

    const fs::path bin = test_bin_root() / "recursive_functions";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

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

    const fs::path bin = test_bin_root() / "method_calls";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path unknown = negative_sample("functions", "unknown_method.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown)).output, "unknown method: unknown_method.Counter.missing");

    const fs::path mutability = negative_sample("functions", "method_receiver_mutability.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mutability)).output, "mutable method receiver requires writable storage");

    const fs::path self_type = negative_sample("functions", "method_self_type.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(self_type)).output, "method self parameter must use the impl type or a pointer to it");

    const fs::path receiver_required = negative_sample("functions", "associated_receiver_required.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(receiver_required)).output, "method requires a receiver");
}

} // namespace aurex::test
