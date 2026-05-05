#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, M1BlockExpression) {
    const fs::path source = source_root() / "tests" / "m1" / "positive" / "block_expression.ax";

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "block_expr",
        "stmt #",
        "let inner\n",
        "var total\n",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn adjust -> i32",
        "fn main -> i32",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "alloca inner",
        "alloca total",
        "store",
        "phi [^if.expr.then",
    });

    const fs::path bin = test_bin_root() / "m1_block_expression";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path empty = source_root() / "tests" / "m1" / "negative" / "block_expression_empty.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(empty)).output, "block expression requires a final expression");

    const fs::path void_result = source_root() / "tests" / "m1" / "negative" / "block_expression_void.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(void_result)).output, "block expression result cannot be void");

    const fs::path scope = source_root() / "tests" / "m1" / "negative" / "block_expression_scope.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(scope)).output, "unknown name: inner");

    const fs::path const_initializer = source_root() / "tests" / "m1" / "negative" / "block_expression_const_initializer.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(const_initializer)).output, "block expression cannot be used in const initializer");
}

} // namespace aurex::test
