#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, IfExpression) {
    const fs::path source = positive_sample("expressions", "if_expression.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "expr #",
        "if_expr",
        "let value\n",
        "let nested\n",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn pick -> i32",
        "fn normalize -> i32",
        "fn main -> i32",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "^if.expr.then",
        "^if.expr.else",
        "^if.expr.join",
        "phi [^if.expr.then",
        "fn normalize(value: i32)",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path condition = negative_sample("expressions", "if_expression_condition.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(condition)).output, "if expression condition must be bool");

    const fs::path mismatch = negative_sample("expressions", "if_expression_type_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "if expression branches must have the same type");

    const fs::path missing_else = negative_sample("expressions", "if_expression_missing_else.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(missing_else)).output, "if expression requires else branch");

    const fs::path const_initializer = negative_sample("expressions", "if_expression_const_initializer.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(const_initializer)).output, "if expression cannot be used in const initializer");
}

} // namespace aurex::test
