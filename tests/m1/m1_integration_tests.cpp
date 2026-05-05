#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, M1TypeAliasPrototype) {
    const fs::path alias = source_root() / "tests" / "m1" / "positive" / "type_alias.ax";
    const std::string alias_tokens = require_success(aurexc() + " --dump-tokens " + q(alias)).output;
    expect_contains(alias_tokens, "kw_type `type`");

    const std::string alias_ast = require_success(aurexc() + " --emit=ast " + q(alias)).output;
    expect_contains_all(alias_ast, {
        "item #0 type_alias Count",
        "alias i32",
        "item #1 type_alias CountPtr",
        "alias *mut Count",
        "item #2 type_alias Bytes4",
        "alias [4]u8",
        "item #3 type_alias PacketAlias",
        "alias Packet",
    });

    const std::string alias_checked = require_success(aurexc() + " --emit=checked " + q(alias)).output;
    expect_contains_all(alias_checked, {
        "type_aliases 4",
        "type Count = i32",
        "type CountPtr = *mut i32",
        "type Bytes4 = [4]u8",
        "type PacketAlias = type_alias.Packet",
    });

    const std::string alias_ir = require_success(aurexc() + " --emit=ir " + q(alias)).output;
    expect_contains_all(alias_ir, {
        "fn read_count(value: *mut i32)",
        "fn main()",
        "record Packet @m0_type_alias_Packet",
        ".len: i32",
    });

    const std::string alias_llvm = require_success(aurexc() + " --emit=llvm-ir " + q(alias)).output;
    expect_contains(alias_llvm, "%m0_type_alias_Packet = type { i32 }");

    const fs::path alias_bin = test_bin_root() / "m1_type_alias";
    require_success(aurexc() + " " + q(alias) + " -o " + q(alias_bin));
    EXPECT_EQ(require_success(q(alias_bin)).output, "");

    const fs::path imported = source_root() / "tests" / "m1" / "positive" / "type_alias_import.ax";
    const fs::path imported_bin = test_bin_root() / "m1_type_alias_import";
    require_success(aurexc() + " -I " + q(source_root() / "tests" / "m1" / "imports") + " " + q(imported) + " -o " + q(imported_bin));
    EXPECT_EQ(require_success(q(imported_bin)).output, "");

    for (const fs::path& src : sorted_files(source_root() / "tests" / "m1" / "negative", ".ax")) {
        const CommandResult result = require_failure(aurexc() + " --check " + q(src));
        if (stem(src) == "type_alias_cycle") {
            expect_contains(result.output, "cyclic type alias");
        }
        if (stem(src) == "type_alias_duplicate") {
            expect_contains(result.output, "duplicate type definition");
        }
        if (stem(src) == "type_alias_opaque_value") {
            expect_contains(result.output, "opaque struct can only be used as a pointer target");
        }
    }
}

TEST_F(AurexIntegrationTest, M1LocalTypeInferencePrototype) {
    const fs::path source = source_root() / "tests" / "m1" / "positive" / "local_inference.ax";

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

    const fs::path bin = test_bin_root() / "m1_local_inference";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path null_source = source_root() / "tests" / "m1" / "negative" / "local_inference_null.ax";
    const CommandResult null_result = require_failure(aurexc() + " --check " + q(null_source));
    expect_contains(null_result.output, "local variable type cannot be inferred");
}

TEST_F(AurexIntegrationTest, M1FunctionReturnInferencePrototype) {
    const fs::path source = source_root() / "tests" / "m1" / "positive" / "return_inference.ax";

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

    const fs::path bin = test_bin_root() / "m1_return_inference";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path mismatch = source_root() / "tests" / "m1" / "negative" / "return_inference_mismatch.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "inferred function return types do not match");

    const fs::path null_source = source_root() / "tests" / "m1" / "negative" / "return_inference_null.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(null_source)).output, "function return type cannot be inferred");

    const fs::path recursive = source_root() / "tests" / "m1" / "negative" / "return_inference_recursive.ax";
    expect_contains(
        require_failure(aurexc() + " --check " + q(recursive)).output,
        "cannot infer recursive function return type without an explicit return type"
    );
}

TEST_F(AurexIntegrationTest, M1FunctionPrototypePrototype) {
    const fs::path source = source_root() / "tests" / "m1" / "positive" / "function_prototype.ax";

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

    const fs::path bin = test_bin_root() / "m1_function_prototype";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path mismatch = source_root() / "tests" / "m1" / "negative" / "function_prototype_mismatch.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(mismatch)).output, "function prototype and definition signatures do not match");

    const fs::path duplicate = source_root() / "tests" / "m1" / "negative" / "function_prototype_duplicate.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output, "duplicate function prototype");

    const fs::path missing = source_root() / "tests" / "m1" / "negative" / "function_prototype_missing_definition.ax";
    expect_contains(require_failure(aurexc() + " --check " + q(missing)).output, "function prototype has no definition");
}

TEST_F(AurexIntegrationTest, M1RecursiveFunctions) {
    const fs::path source = source_root() / "tests" / "m1" / "positive" / "recursive_functions.ax";

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

    const fs::path bin = test_bin_root() / "m1_recursive_functions";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path inferred = source_root() / "tests" / "m1" / "negative" / "recursive_return_inference.ax";
    expect_contains(
        require_failure(aurexc() + " --check " + q(inferred)).output,
        "cannot infer recursive function return type without an explicit return type"
    );
}

} // namespace aurex::test
