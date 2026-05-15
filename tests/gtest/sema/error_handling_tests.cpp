#include <support/test_support.hpp>

namespace aurex::test {

TEST_F(AurexIntegrationTest, TryExpressionResultAndOption) {
    const fs::path source = positive_sample("error_handling", "try_result_option.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "expr #",
        "postfix_op try",
        "name `ResultI32I32`",
        "name `OptionI32`",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn priv add_one_result -> try_result_option.ResultI32I32",
        "fn priv unwrap_even -> try_result_option.OptionI32",
        "case ResultI32I32_ok",
        "case OptionI32_some",
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
        "try expression requires result-like ok/err enum or option-like some/none enum"
    );

    const fs::path return_mismatch = negative_sample("error_handling", "try_result_return_mismatch.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(return_mismatch)).output,
        "try expression on result-like enum requires enclosing function to return result-like enum with err payload"
    );

    const fs::path error_mismatch = negative_sample("error_handling", "try_result_error_mismatch.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(error_mismatch)).output,
        "try expression result-like enum err payload type must match enclosing result-like enum err payload type"
    );

    const fs::path malformed_result = negative_sample("error_handling", "try_result_malformed.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(malformed_result)).output,
        "try expression result-like enum must define ok(payload) and err(payload) cases"
    );

    const fs::path malformed_option = negative_sample("error_handling", "try_option_malformed.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(malformed_option)).output,
        "try expression option-like enum must define some(payload) and none cases"
    );

    const fs::path extra_result_case = negative_sample("error_handling", "try_result_extra_case.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(extra_result_case)).output,
        "try expression requires result-like ok/err enum or option-like some/none enum"
    );

    const fs::path extra_option_case = negative_sample("error_handling", "try_option_extra_case.ax");
    expect_contains(
        require_failure(aurexc() + " --check " + q(extra_option_case)).output,
        "try expression requires result-like ok/err enum or option-like some/none enum"
    );
}

} // namespace aurex::test
