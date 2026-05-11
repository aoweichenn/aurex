#include <support/test_support.hpp>

namespace aurex::test {

TEST_F(AurexIntegrationTest, MatchExpression) {
    const fs::path source = positive_sample("pattern_matching", "match_expression.ax");

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
        "fn priv score -> i32",
        "fn priv main -> i32",
        "enum_cases 2",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "^match.arm",
        "^match.join",
        "phi [^match.arm",
        "const_ref @m0_match_expression_Choice_zero",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path non_enum = negative_sample("pattern_matching", "match_expression_non_enum.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(non_enum)).output, "match expression requires an enum, integer, or bool value");

    const fs::path missing = negative_sample("pattern_matching", "match_expression_missing_case.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(missing)).output, "match expression is not exhaustive");

    const fs::path duplicate = negative_sample("pattern_matching", "match_expression_duplicate.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output, "duplicate match arm");

    const fs::path mismatch = negative_sample("pattern_matching", "match_expression_type_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "match expression arms must have the same type");
}

TEST_F(AurexIntegrationTest, EnumPayloadAndMatchBinding) {
    const fs::path source = positive_sample("pattern_matching", "enum_payload.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "case int(i32) = 1",
        "case pair(Pair) = 2",
        "match_arm .int(value)",
        "match_arm .pair(pair)",
        "match_arm .none",
    });

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains(checked, "enum_cases 3");

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "record enum_payload.Packet",
        ".tag: u8",
        ".payload:",
        "field_addr",
        "enum.payload.storage",
        "field_addr %4.tag",
        "aggregate {.tag",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path missing_arg = negative_sample("pattern_matching", "enum_payload_constructor_missing_arg.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(missing_arg)).output, "enum payload constructor requires exactly one argument");

    const fs::path scoped_missing_arg = negative_sample("pattern_matching", "enum_scoped_constructor_missing_arg.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(scoped_missing_arg)).output, "enum payload constructor requires exactly one argument");

    const fs::path wrong_type = negative_sample("pattern_matching", "enum_payload_constructor_wrong_type.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(wrong_type)).output, "enum payload constructor argument type mismatch");

    const fs::path empty_binding = negative_sample("pattern_matching", "enum_payload_binding_on_empty_case.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(empty_binding)).output, "match arm payload binding requires a payload enum case");

    const fs::path unknown_binding = negative_sample("pattern_matching", "enum_payload_unknown_binding.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown_binding)).output, "unknown name: missing");

    const fs::path array_storage = negative_sample("pattern_matching", "enum_payload_array_storage.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(array_storage)).output, "enum payload cannot contain array storage");
}

TEST_F(AurexIntegrationTest, MatchWildcardAndScopedCases) {
    const fs::path source = positive_sample("pattern_matching", "match_wildcard.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "match_arm .fast",
        "match_arm _",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "^match.arm",
        "^match.join",
        "phi [^match.arm",
        "const_ref @m0_match_wildcard_Mode_fast",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path unreachable = negative_sample("pattern_matching", "match_wildcard_unreachable.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unreachable)).output, "match arm is unreachable after wildcard pattern");

    const fs::path wrong_enum = negative_sample("pattern_matching", "match_scoped_wrong_enum.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(wrong_enum)).output, "match arm case does not belong to matched enum");
}

TEST_F(AurexIntegrationTest, MatchOrPattern) {
    const fs::path source = positive_sample("pattern_matching", "match_or_pattern.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains(ast, "match_arm .red | .green");

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "const_ref @m0_match_or_pattern_Light_red",
        "const_ref @m0_match_or_pattern_Light_green",
        "or %",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path payload_binding = negative_sample("pattern_matching", "match_or_pattern_payload_binding.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(payload_binding)).output, "or-pattern alternatives cannot bind payloads");

    const fs::path duplicate = negative_sample("pattern_matching", "match_or_pattern_duplicate.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output, "duplicate match arm for enum case");
}

TEST_F(AurexIntegrationTest, MatchLiteralPattern) {
    const fs::path source = positive_sample("pattern_matching", "match_literal_pattern.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "match_arm 0",
        "match_arm 1 | 2",
        "match_arm true",
        "match_arm false",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "literal 0",
        "literal 1",
        "literal 2",
        "or %",
        "literal true",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path missing_wildcard = negative_sample("pattern_matching", "match_literal_missing_wildcard.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(missing_wildcard)).output, "match expression over integer or bool requires a wildcard arm");

    const fs::path type_mismatch = negative_sample("pattern_matching", "match_literal_type_mismatch.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(type_mismatch)).output, "bool match pattern must be true or false");

    const fs::path enum_literal = negative_sample("pattern_matching", "match_enum_literal_pattern.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(enum_literal)).output, "enum match pattern must be an enum case or wildcard");
}

TEST_F(AurexIntegrationTest, MatchGuard) {
    const fs::path source = positive_sample("pattern_matching", "match_guard.ax");

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(source)).output;
    expect_contains_all(ast, {
        "match_arm .int(value)",
        "guard",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "^match.guard.pass",
        "gt %",
        "field_addr",
        "match.next",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path non_bool = negative_sample("pattern_matching", "match_guard_non_bool.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(non_bool)).output, "match guard must be bool");

    const fs::path not_exhaustive = negative_sample("pattern_matching", "match_guard_not_exhaustive.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(not_exhaustive)).output, "match expression is not exhaustive");

    const fs::path unknown_binding = negative_sample("pattern_matching", "match_guard_unknown_binding.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown_binding)).output, "unknown name: missing");
}

TEST_F(AurexIntegrationTest, LayoutAlignment) {
    const fs::path source = positive_sample("types", "layout_alignment.ax");

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "sizeof layout_alignment.Padded",
        "alignof layout_alignment.Padded",
        "sizeof layout_alignment.Tag",
        "alignof layout_alignment.Tag",
        "sizeof layout_alignment.Payload",
        "alignof layout_alignment.Payload",
    });

    require_success(aurexc() + " --emit=llvm-ir " + q(source));
}

} // namespace aurex::test
