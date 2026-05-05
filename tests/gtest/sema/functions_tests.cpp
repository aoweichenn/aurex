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

} // namespace aurex::test
