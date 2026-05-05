#include "support/test_support.hpp"

#include <set>
#include <string>

namespace aurex::test {

TEST_F(AurexIntegrationTest, PositiveAndNegativeSamples) {
    const std::set<std::string> skip_regular = {
        "import_alias_qualified_call",
        "import_path",
        "generic_function_import",
        "math",
        "module_name_collision",
        "reexport_import",
        "std_text",
        "std_mem",
        "std_file",
        "type_alias_import",
        "visibility_import",
    };
    const std::set<std::string> run_regular = {
        "condition_regression",
        "pointer_ops",
        "mut_to_const_pointer",
        "address_of_let",
        "pointer_field_write",
        "eval_order_call_stmt",
        "eval_order_return",
        "eval_order_assign",
        "eval_order_condition",
        "builtins",
    };
    for (const fs::path& src : sorted_files(positive_samples_root(), ".ax")) {
        const std::string name = stem(src);
        if (skip_regular.contains(name)) {
            continue;
        }
        const fs::path bin = test_bin_root() / name;
        require_success(aurexc() + " " + q(src) + " -o " + q(bin));
        if (run_regular.contains(name)) {
            require_success(q(bin));
        }
    }

    for (const fs::path& src : sorted_files(positive_samples_root(), ".ax")) {
        const std::string name = stem(src);
        if (name.rfind("std_", 0) != 0) {
            continue;
        }
        const fs::path bin = test_bin_root() / name;
        const fs::path direct = test_bin_root() / (name + ".direct");
        require_success(aurexc() + " " + q(src) + " -o " + q(bin));
        require_success(q(bin));
        require_success(aurexc() + " --emit=exe " + q(src) + " -o " + q(direct));
        require_success(q(direct));
    }

    const std::string const_enum =
        require_success(aurexc() + " --emit=llvm-ir " + q(positive_sample("types", "const_enum.ax"))).output;
    expect_contains(const_enum, "@m0_const_enum_answer = internal unnamed_addr constant i32 42");
    expect_contains(const_enum, "load i32, ptr @m0_const_enum_answer");

    for (const fs::path& src : sorted_files(negative_samples_root(), ".ax")) {
        std::string command = aurexc() + " --check " + q(src);
        if (stem(src) == "module_name_mismatch" || stem(src) == "cyclic_import" || stem(src) == "ambiguous_import_name") {
            command = aurexc() + " " + tests_import_flags() + " --check " + q(src);
        }
        const CommandResult result = require_failure(command);
        if (stem(src) == "ambiguous_import_name") {
            expect_contains(result.output, "ambiguous function name");
        }
    }
}

TEST_F(AurexIntegrationTest, StdCollectionsPathSampleExposesM1ContainerBaseline) {
    const fs::path source = positive_sample("std", "std_collections_path.ax");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "type VecU8 = std.core.vec.Vec<u8>",
        "fn std.core.vec.new<u8> -> std.core.vec.Vec<u8>",
        "fn std.core.vec.push<u8> -> bool",
        "fn std.core.vec.as_span<u8> -> std.core.text.Span<u8>",
        "fn from_c -> std.core.result.Result<std.core.string.String, i32>",
        "fn join_c -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn method std.core.vec.Vec<u8>.push -> bool",
        "fn method std.core.string.String.from_c -> std.core.result.Result<std.core.string.String, i32>",
        "fn method std.fs.path.Path.join_c -> std.core.result.Result<std.fs.path.Path, i32>",
        "fn run -> std.core.result.Result<i32, i32>",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "record std.core.vec.Vec<u8>",
        "record String",
        "record Path",
        "call m0_std_core_vec_push__u8",
        "call m0_std_core_string_from_c",
        "call m0_std_fs_path_join_c",
        "try.ok",
        "try.err",
    });

    const fs::path bin = test_bin_root() / "std_collections_path_explicit";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

TEST_F(AurexIntegrationTest, StdTextSampleExposesGenericSpanBaseline) {
    const fs::path source = positive_sample("std", "std_text.ax");

    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "generic_functions 4",
        "fn std.core.text.span<u8> -> std.core.text.Span<u8>",
        "fn std.core.text.mut_span<u8> -> std.core.text.MutSpan<u8>",
        "fn std.core.text.span<i32> -> std.core.text.Span<i32>",
        "fn std.core.text.mut_span<i32> -> std.core.text.MutSpan<i32>",
        "struct std.core.text.Span<i32> fields=2",
        "struct std.core.text.MutSpan<i32> fields=2",
        "type SpanU8 = std.core.text.Span<u8>",
        "type MutSpanU8 = std.core.text.MutSpan<u8>",
    });

    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "record std.core.text.Span<i32>",
        "record std.core.text.MutSpan<i32>",
        "fn std.core.text.span<i32>(data: *const i32, len: usize)",
        "fn std.core.text.mut_span<i32>(data: *mut i32, len: usize)",
        "call m0_std_core_text_span__i32",
        "call m0_std_core_text_mut_span__i32",
    });

    const fs::path bin = test_bin_root() / "std_text_generic_span";
    require_success(aurexc() + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");
}

} // namespace aurex::test
