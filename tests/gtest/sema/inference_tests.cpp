#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, LocalTypeInference) {
    const fs::path source = positive_sample("inference", "local_inference.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "var value\n",
        "let ptr\n",
        "let pair\n",
        "let aliased : CountPtr",
    });
    expect_not_contains(ast, "var value :");
    expect_not_contains(ast, "let ptr :");
    expect_not_contains(ast, "let pair :");

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn make_count()",
        ": *mut i32 = alloca value",
        ": *mut *mut i32 = alloca ptr",
        ": *mut local_inference.Pair = alloca pair",
        ": *mut *mut i32 = alloca aliased",
    });

    const fs::path bin = test_bin_root() / "local_inference";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path null_source = negative_sample("inference", "local_inference_null.ax");
    const CommandResult null_result = require_failure(aurexc() + " --check " + q(null_source));
    expect_contains(null_result.output, "local variable type cannot be inferred");
}

TEST_F(AurexIntegrationTest, FunctionReturnInference) {
    const fs::path source = positive_sample("inference", "return_inference.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "item #2 fn make_count\n",
        "item #3 fn make_pair\n",
        "item #4 fn touch\n",
        "item #5 fn choose\n",
    });
    expect_not_contains(ast, "fn make_count\n    return");
    expect_not_contains(ast, "fn make_pair\n    return");
    expect_not_contains(ast, "fn touch\n    return");
    expect_not_contains(ast, "fn choose\n    return");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn make_count -> i32",
        "fn make_pair -> return_inference.Pair",
        "fn touch -> void",
        "fn choose -> i32",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn make_count()",
        "-> i32",
        "fn make_pair(value: i32)",
        "-> return_inference.Pair",
        "fn touch(value: *mut i32)",
        "-> void",
        "fn choose(flag: bool, value: i32)",
    });

    const fs::path bin = test_bin_root() / "return_inference";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path mismatch = negative_sample("inference", "return_inference_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "inferred function return types do not match");

    const fs::path null_source = negative_sample("inference", "return_inference_null.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(null_source)).output, "function return type cannot be inferred");

    const fs::path recursive = negative_sample("inference", "return_inference_recursive.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(recursive)).output,
        "cannot infer recursive function return type without an explicit return type"
    );
}

} // namespace aurex::test
