#include <support/test_support.hpp>

namespace aurex::test {

TEST_F(AurexIntegrationTest, TryExpressionResultAndOption) {
    const fs::path source = positive_sample("error_handling", "try_result_option.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "expr #",
        "try_expr",
        "name `Result`",
        "name `Option`",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn add_one_result -> try_result_option.Result<i32, i32>",
        "fn unwrap_even -> try_result_option.Option<i32>",
        "case Result<i32, i32>_ok",
        "case Option<i32>_some",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "try.ok",
        "try.err",
        "try.join",
        "ret %",
        "field_addr",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));
}

TEST_F(AurexIntegrationTest, TryExpressionDiagnostics) {
    const fs::path non_result = negative_sample("error_handling", "try_non_result.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(non_result)).output,
        "try expression requires Result<T, E> or Option<T>"
    );

    const fs::path return_mismatch = negative_sample("error_handling", "try_result_return_mismatch.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(return_mismatch)).output,
        "try expression on Result<T, E> requires enclosing function to return Result<U, E>"
    );

    const fs::path error_mismatch = negative_sample("error_handling", "try_result_error_mismatch.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(error_mismatch)).output,
        "try expression Result error type must match enclosing Result error type"
    );

    const fs::path malformed_result = negative_sample("error_handling", "try_result_malformed.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(malformed_result)).output,
        "try expression Result type must define ok(T) and err(E) cases"
    );

    const fs::path malformed_option = negative_sample("error_handling", "try_option_malformed.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(malformed_option)).output,
        "try expression Option type must define some(T) and none cases"
    );
}

} // namespace aurex::test
