#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, M1MatchExpression) {
    const fs::path source = source_root() / "tests" / "m1" / "positive" / "match_expression.ax";

    const std::string tokens = require_success(aurexc() + " --dump-tokens " + q(source)).output;
    expect_contains_all(tokens, {
        "kw_match `match`",
        "fat_arrow `=>`",
    });

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "match_expr",
        "match_arm Choice_zero",
        "match_arm Choice_one",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn score -> i32",
        "fn main -> i32",
        "enum_cases 2",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "^match.arm",
        "^match.join",
        "phi [^match.arm",
        "const_ref @m0_match_expression_Choice_zero",
    });

    const fs::path bin = test_bin_root() / "m1_match_expression";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path non_enum = source_root() / "tests" / "m1" / "negative" / "match_expression_non_enum.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(non_enum)).output, "match expression requires an enum value");

    const fs::path missing = source_root() / "tests" / "m1" / "negative" / "match_expression_missing_case.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(missing)).output, "match expression is not exhaustive");

    const fs::path duplicate = source_root() / "tests" / "m1" / "negative" / "match_expression_duplicate.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output, "duplicate match arm");

    const fs::path mismatch = source_root() / "tests" / "m1" / "negative" / "match_expression_type_mismatch.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "match expression arms must have the same type");
}

TEST_F(AurexIntegrationTest, M1LayoutAlignment) {
    const fs::path source = source_root() / "tests" / "m1" / "positive" / "layout_alignment.ax";

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "size_of layout_alignment.Padded",
        "align_of layout_alignment.Padded",
        "size_of layout_alignment.Tag",
        "align_of layout_alignment.Tag",
    });

    const fs::path bin = test_bin_root() / "m1_layout_alignment";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

} // namespace aurex::test
