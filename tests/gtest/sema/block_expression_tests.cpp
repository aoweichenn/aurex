#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, BlockExpression) {
    const fs::path source = positive_sample("expressions", "block_expression.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "block_expr",
        "stmt #",
        "let inner\n",
        "var total\n",
        "while",
        "for",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn adjust -> i32",
        "fn inferred_return -> i32",
        "fn main -> i32",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "alloca inner",
        "alloca total",
        "store",
        "phi [^if.expr.then",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path empty = negative_sample("expressions", "block_expression_empty.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(empty)).output, "block expression requires a final expression");

    const fs::path void_result = negative_sample("expressions", "block_expression_void.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(void_result)).output, "block expression result cannot be void");

    const fs::path scope = negative_sample("expressions", "block_expression_scope.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(scope)).output, "unknown name: inner");

    const fs::path const_initializer = negative_sample("expressions", "block_expression_const_initializer.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(const_initializer)).output, "block expression cannot be used in const initializer");

    const fs::path unreachable_tail = negative_sample("expressions", "block_expression_unreachable_tail.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unreachable_tail)).output, "block expression final expression is unreachable");
}

} // namespace aurex::test
