#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sys/wait.h>

namespace {

namespace fs = std::filesystem;

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

fs::path source_root() {
    return fs::path(AUREX_TEST_SOURCE_DIR);
}

fs::path build_root() {
    return fs::path(AUREX_TEST_BINARY_DIR);
}

fs::path work_root() {
    return build_root() / "gtest";
}

fs::path test_bin_root() {
    return work_root() / "tests";
}

fs::path selfhost_bin_root() {
    return work_root() / "selfhost";
}

fs::path tmp_root() {
    return work_root() / "tmp";
}

fs::path aurexc_path() {
    return build_root() / "bin" / "aurexc";
}

std::string shell_quote(const std::string_view value) {
    std::string quoted = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\"'\"'";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

std::string q(const fs::path& path) {
    return shell_quote(path.string());
}

std::string q(const std::string_view value) {
    return shell_quote(value);
}

int decode_status(const int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return status;
}

CommandResult run_command(const std::string& command) {
    std::array<char, 4096> buffer {};
    std::string output;
    const std::string full_command = command + " 2>&1";
    FILE* pipe = popen(full_command.c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error("failed to start command: " + command);
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    const int status = pclose(pipe);
    return CommandResult {decode_status(status), output};
}

CommandResult require_success(const std::string& command) {
    CommandResult result = run_command(command);
    if (result.exit_code != 0) {
        throw std::runtime_error(
            "command failed with exit code " + std::to_string(result.exit_code) + "\n" +
            command + "\n" + result.output
        );
    }
    return result;
}

CommandResult require_failure(const std::string& command) {
    CommandResult result = run_command(command);
    if (result.exit_code == 0) {
        throw std::runtime_error("command unexpectedly succeeded\n" + command + "\n" + result.output);
    }
    return result;
}

std::string read_text(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void expect_contains(const std::string_view text, const std::string_view needle) {
    EXPECT_NE(text.find(needle), std::string_view::npos) << "missing: " << needle;
}

void expect_contains_all(const std::string_view text, const std::vector<std::string_view>& needles) {
    for (const std::string_view needle : needles) {
        expect_contains(text, needle);
    }
}

void expect_not_contains(const std::string_view text, const std::string_view needle) {
    EXPECT_EQ(text.find(needle), std::string_view::npos) << "unexpected: " << needle;
}

int count_lines_starting_with(const std::string_view text, const std::string_view prefix) {
    std::istringstream input {std::string(text)};
    std::string line;
    int count = 0;
    while (std::getline(input, line)) {
        if (line.rfind(prefix, 0) == 0) {
            ++count;
        }
    }
    return count;
}

std::vector<fs::path> sorted_files(const fs::path& dir, const std::string_view extension) {
    std::vector<fs::path> files;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string stem(const fs::path& path) {
    return path.stem().string();
}

std::string aurexc() {
    return q(aurexc_path());
}

std::string selfhost_import_flags() {
    return "-I " + q(source_root() / "selfhost" / "src");
}

std::string tests_import_flags() {
    return "-I " + q(source_root() / "tests" / "imports");
}

fs::path compile_selfhost_program(const std::string_view name, const fs::path& source) {
    fs::create_directories(selfhost_bin_root());
    const fs::path output = selfhost_bin_root() / std::string(name);
    require_success(aurexc() + " " + selfhost_import_flags() + " " + q(source) + " -o " + q(output));
    return output;
}

fs::path compile_stage1() {
    return compile_selfhost_program(
        "aurexc_stage1",
        source_root() / "selfhost" / "src" / "aurex" / "selfhost" / "bin" / "aurexc_stage1.ax"
    );
}

fs::path run_stage1(const fs::path& stage1_bin, const fs::path& source, const std::string_view out_name) {
    fs::create_directories(selfhost_bin_root());
    const fs::path output = selfhost_bin_root() / std::string(out_name);
    require_success(q(stage1_bin) + " " + q(source) + " " + q(output));
    return output;
}

std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> result;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            result.push_back(line);
        }
    }
    return result;
}

std::vector<std::string> stage0_token_kinds(const fs::path& source) {
    const CommandResult result = require_success(aurexc() + " --dump-tokens " + q(source));
    std::vector<std::string> kinds;
    std::istringstream input(result.output);
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string range;
        std::string kind;
        fields >> range >> kind;
        if (!kind.empty()) {
            kinds.push_back(kind);
        }
    }
    return kinds;
}

std::vector<fs::path> stage_compiler_sources() {
    const fs::path base = source_root() / "selfhost" / "src" / "aurex" / "selfhost";
    return {
        base / "lexer" / "core.ax",
        base / "syntax" / "ast.ax",
        base / "parser" / "cursor.ax",
        base / "parser" / "types.ax",
        base / "parser" / "expr.ax",
        base / "parser" / "seed.ax",
        base / "sema" / "names.ax",
        base / "sema" / "calls.ax",
        base / "sema" / "items.ax",
        base / "sema" / "locals.ax",
        base / "sema" / "lvalues.ax",
        base / "sema" / "members.ax",
        base / "sema" / "resolve.ax",
        base / "sema" / "typing_types.ax",
        base / "sema" / "typing_lookup.ax",
        base / "sema" / "typing_infer.ax",
        base / "sema" / "annotate.ax",
        base / "sema" / "typing.ax",
        base / "sema" / "types.ax",
        base / "compiler" / "air" / "model.ax",
        base / "compiler" / "air" / "bind.ax",
        base / "compiler" / "air" / "place.ax",
        base / "compiler" / "air" / "memory.ax",
        base / "compiler" / "air" / "flow.ax",
        base / "compiler" / "air" / "cfg.ax",
        base / "compiler" / "air" / "lower.ax",
        base / "compiler" / "air" / "text.ax",
        base / "compiler" / "air" / "verify.ax",
        base / "compiler" / "io.ax",
        base / "compiler" / "ir" / "writer.ax",
        base / "compiler" / "ir" / "names.ax",
        base / "compiler" / "ir" / "types.ax",
        base / "compiler" / "ir" / "expr.ax",
        base / "compiler" / "ir" / "cfg.ax",
        base / "compiler" / "ir" / "emit.ax",
        base / "compiler" / "driver.ax",
        base / "bin" / "aurexc_stage1.ax",
    };
}

class AurexIntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        fs::create_directories(test_bin_root());
        fs::create_directories(selfhost_bin_root());
        fs::create_directories(tmp_root());
        if (!fs::exists(aurexc_path())) {
            throw std::runtime_error("missing aurexc binary: " + aurexc_path().string());
        }
    }
};

TEST_F(AurexIntegrationTest, DocumentationLayoutIsStable) {
    const std::vector<fs::path> required = {
        "docs/README.md",
        "docs/zh/README.md",
        "docs/zh/architecture.md",
        "docs/zh/requirements.md",
        "docs/zh/runtime-flow.md",
        "docs/zh/api.md",
        "docs/zh/implementation.md",
        "docs/zh/usage.md",
        "docs/zh/introduction.md",
        "docs/zh/version.md",
        "docs/zh/next-steps.md",
        "docs/en/README.md",
        "docs/en/architecture.md",
        "docs/en/requirements.md",
        "docs/en/runtime-flow.md",
        "docs/en/api.md",
        "docs/en/implementation.md",
        "docs/en/usage.md",
        "docs/en/introduction.md",
        "docs/en/version.md",
        "docs/en/next-steps.md",
    };
    for (const fs::path& path : required) {
        EXPECT_TRUE(fs::exists(source_root() / path)) << path;
    }

    const std::vector<fs::path> obsolete = {
        "docs/ARCHITECTURE.zh.md",
        "docs/DESIGN.en.md",
        "docs/DESIGN.zh.md",
        "docs/SELFHOST.md",
        "docs/SEMANTICS.md",
        "docs/USAGE.en.md",
        "docs/USAGE.zh.md",
    };
    for (const fs::path& path : obsolete) {
        EXPECT_FALSE(fs::exists(source_root() / path)) << path;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(source_root() / "docs")) {
        EXPECT_FALSE(entry.path().filename().string().rfind("M0V0.1.", 0) == 0) << entry.path();
    }
}

TEST_F(AurexIntegrationTest, CliAndFrontendDumps) {
    expect_contains(require_success(aurexc() + " --version").output, "0.1.2");

    const std::string help = require_success(aurexc() + " --help").output;
    expect_contains_all(help, {
        "--check",
        "--emit=ast",
        "--emit=ir",
        "--emit=llvm-ir",
        "--emit=asm",
        "--emit=obj",
        "--emit=exe",
        "--no-stdlib",
        "--dump-modules",
        "--opt-level",
        "--stdlib",
        "--std-backend",
    });

    const fs::path hello = source_root() / "examples" / "hello.ax";
    require_success(aurexc() + " --check " + q(hello));
    require_success(aurexc() + " --emit=check " + q(hello));

    const std::string tokens = require_success(aurexc() + " --dump-tokens " + q(hello)).output;
    const std::string ast = require_success(aurexc() + " --emit=ast " + q(hello)).output;
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(hello)).output;
    const std::string ir = require_success(aurexc() + " --emit=ir " + q(hello)).output;
    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(hello)).output;

    EXPECT_EQ(read_text(source_root() / "tests" / "golden" / "hello.tokens"), tokens);
    expect_contains(tokens, "c_string_literal");
    expect_contains(ast, "extern_block");
    expect_contains(checked, "checked_module");
    expect_contains(ir, "aurex_ir v0");
    expect_contains(ir, "fn puts(s: *const u8) @puts linkage(extern_c) abi(c) -> i32");
    expect_contains(ir, "call puts");
    expect_contains(llvm_ir, "define i32 @main");

    const std::string eval_order =
        require_success(aurexc() + " --emit=ir --opt-level O1 " + q(source_root() / "tests" / "positive" / "eval_order_assign.ax")).output;
    expect_contains(eval_order, "call m0_eval_order_assign_next(%");

    const std::string std_text =
        require_success(aurexc() + " --emit=ir " + q(source_root() / "tests" / "positive" / "std_text.ax")).output;
    expect_contains(std_text, "phi [");
    expect_contains(std_text, "usize = cast");

    const std::string pointer_field =
        require_success(aurexc() + " --emit=ir " + q(source_root() / "tests" / "positive" / "pointer_field_write.ax")).output;
    expect_contains(pointer_field, "field_addr ");
    expect_contains(pointer_field, ".value");

    const std::string selfhost_lexer_llvm =
        require_success(aurexc() + " " + selfhost_import_flags() + " --emit=llvm-ir " +
                        q(source_root() / "selfhost" / "src" / "aurex" / "selfhost" / "tool" / "lexer_file.ax")).output;
    expect_contains(selfhost_lexer_llvm, "aurex_std_v0_read_file");
}

TEST_F(AurexIntegrationTest, NativeHelloOutputs) {
    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path hello_bin = test_bin_root() / "hello";
    require_success(aurexc() + " " + q(hello) + " -o " + q(hello_bin));
    EXPECT_EQ(require_success(q(hello_bin)).output, "hello from Aurex M0\n");

    const fs::path asm_out = test_bin_root() / "hello.s";
    require_success(aurexc() + " --emit=asm " + q(hello) + " -o " + q(asm_out));
    EXPECT_GT(fs::file_size(asm_out), 0U);

    const fs::path obj_out = test_bin_root() / "hello.o";
    require_success(aurexc() + " --emit=obj " + q(hello) + " -o " + q(obj_out));
    EXPECT_GT(fs::file_size(obj_out), 0U);

    const fs::path direct = test_bin_root() / "hello.direct";
    require_success(aurexc() + " --emit=exe " + q(hello) + " -o " + q(direct));
    EXPECT_EQ(require_success(q(direct)).output, "hello from Aurex M0\n");

    const fs::path stdnone = test_bin_root() / "hello.stdnone";
    require_success(aurexc() + " --std-backend none " + q(hello) + " -o " + q(stdnone));
    EXPECT_EQ(require_success(q(stdnone)).output, "hello from Aurex M0\n");
}

TEST_F(AurexIntegrationTest, PositiveAndNegativeSamples) {
    const std::set<std::string> skip_regular = {
        "import_path",
        "module_name_collision",
        "std_text",
        "std_mem",
        "std_file",
    };
    const std::set<std::string> run_regular = {
        "condition_regression",
        "pointer_ops",
        "address_of_let",
        "pointer_field_write",
        "eval_order_call_stmt",
        "eval_order_return",
        "eval_order_assign",
        "eval_order_condition",
        "builtins",
    };
    for (const fs::path& src : sorted_files(source_root() / "tests" / "positive", ".ax")) {
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

    for (const fs::path& src : sorted_files(source_root() / "tests" / "positive", ".ax")) {
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
        require_success(aurexc() + " --emit=llvm-ir " + q(source_root() / "tests" / "positive" / "const_enum.ax")).output;
    expect_contains(const_enum, "@m0_const_enum_answer = internal unnamed_addr constant i32 42");
    expect_contains(const_enum, "load i32, ptr @m0_const_enum_answer");

    for (const fs::path& src : sorted_files(source_root() / "tests" / "negative", ".ax")) {
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

TEST_F(AurexIntegrationTest, InstallAndImportPaths) {
    const fs::path install_root = work_root() / "install";
    require_success(q(std::string_view(AUREX_TEST_CMAKE_COMMAND)) + " --install " + q(build_root()) + " --prefix " + q(install_root));

    const fs::path installed_bin = test_bin_root() / "std_text.installed";
    require_success(q(install_root / "bin" / "aurexc") + " " +
                    q(source_root() / "tests" / "positive" / "std_text.ax") + " -o " + q(installed_bin));
    require_success(q(installed_bin));

    const fs::path import_bin = test_bin_root() / "import_path";
    require_success(aurexc() + " " + tests_import_flags() + " " +
                    q(source_root() / "tests" / "positive" / "import_path.ax") + " -o " + q(import_bin));
    require_success(q(import_bin));

    const std::string modules =
        require_success(aurexc() + " --dump-modules " + q(source_root() / "tests" / "positive" / "module_math.ax")).output;
    expect_contains(modules, "lib.math");
    expect_contains(modules, "module_math");

    const std::string collision_ll =
        require_success(aurexc() + " " + tests_import_flags() + " --emit=llvm-ir " +
                        q(source_root() / "tests" / "positive" / "module_name_collision.ax")).output;
    expect_contains(collision_ll, "@m0_module_name_collision_helper");
    expect_contains(collision_ll, "@m0_collide_a_helper");

    const fs::path collision_bin = test_bin_root() / "module_name_collision";
    require_success(aurexc() + " " + tests_import_flags() + " " +
                    q(source_root() / "tests" / "positive" / "module_name_collision.ax") + " -o " + q(collision_bin));
    require_success(q(collision_bin));
}

TEST_F(AurexIntegrationTest, SelfhostSmokeProgramsAndGoldenLexers) {
    const fs::path selfhost = source_root() / "selfhost" / "src" / "aurex" / "selfhost";
    const std::vector<std::pair<std::string, std::string>> programs = {
        {"aurexc_seed", "bin/aurexc_seed.ax"},
        {"lexer_smoke", "smoke/lexer_smoke.ax"},
        {"lexer_ranges", "smoke/lexer_ranges.ax"},
        {"parser_smoke", "smoke/parser_smoke.ax"},
        {"sema_items", "smoke/sema_items.ax"},
        {"air_verify", "smoke/air_verify.ax"},
        {"air_text", "smoke/air_text.ax"},
        {"stage1_lang", "smoke/stage1_lang.ax"},
        {"stage1_core", "smoke/stage1_core.ax"},
        {"stage1_ir", "smoke/stage1_ir.ax"},
    };

    for (const auto& [name, relative] : programs) {
        require_success(aurexc() + " " + selfhost_import_flags() + " --check " + q(selfhost / relative));
        const fs::path bin = compile_selfhost_program(name, selfhost / relative);
        if (name == "air_text") {
            const fs::path air_out = selfhost_bin_root() / "air_text.air";
            EXPECT_EQ(require_success(q(bin) + " " + q(air_out)).output, "selfhost air text ok\n");
            const std::string text = read_text(air_out);
            expect_contains(text, "block_param %v");
            expect_contains(text, "edge fallthrough ^air0 -> ^air1 args(%v");
        } else if (name == "stage1_ir") {
            require_success(q(bin));
        } else {
            const std::string output = require_success(q(bin)).output;
            if (name == "aurexc_seed") {
                EXPECT_EQ(output, "Aurex M0 selfhost seed\n");
            } else if (name == "lexer_smoke") {
                EXPECT_EQ(output, "selfhost lexer sequence ok\n");
            } else if (name == "lexer_ranges") {
                EXPECT_EQ(output, "selfhost lexer ranges ok\n");
            } else if (name == "parser_smoke") {
                EXPECT_EQ(output, "selfhost parser seed ok\n");
            } else if (name == "sema_items") {
                EXPECT_EQ(output, "selfhost sema items ok\n");
            } else if (name == "air_verify") {
                EXPECT_EQ(output, "selfhost air verifier ok\n");
            } else if (name == "stage1_lang") {
                EXPECT_EQ(output, "selfhost stage1 lang ok\n");
            } else if (name == "stage1_core") {
                EXPECT_EQ(output, "selfhost stage1 core ok\n");
            }
        }
    }

    const std::string lexer_modules =
        require_success(aurexc() + " " + selfhost_import_flags() + " --dump-modules " + q(selfhost / "tool" / "lexer_file.ax")).output;
    expect_contains(lexer_modules, "aurex.selfhost.lexer.dump");
    expect_contains(lexer_modules, "aurex.selfhost.lexer.core");

    const std::string parser_modules =
        require_success(aurexc() + " " + selfhost_import_flags() + " --dump-modules " + q(selfhost / "smoke" / "parser_smoke.ax")).output;
    expect_contains_all(parser_modules, {
        "aurex.selfhost.parser.seed",
        "aurex.selfhost.parser.cursor",
        "aurex.selfhost.parser.expr",
        "aurex.selfhost.parser.types",
        "aurex.selfhost.lexer.core",
        "aurex.selfhost.syntax.ast",
    });

    const fs::path lexer_dump = compile_selfhost_program("lexer_dump", selfhost / "tool" / "lexer_dump.ax");
    EXPECT_EQ(
        read_text(source_root() / "tests" / "golden" / "selfhost_lexer_dump.tokens"),
        require_success(q(lexer_dump)).output
    );

    const fs::path lexer_file = compile_selfhost_program("lexer_file", selfhost / "tool" / "lexer_file.ax");
    EXPECT_EQ(
        read_text(source_root() / "tests" / "golden" / "selfhost_lexer_file_hello.tokens"),
        require_success(q(lexer_file) + " " + q(source_root() / "examples" / "hello.ax")).output
    );
}

TEST_F(AurexIntegrationTest, SelfhostLexerMatchesStage0Corpus) {
    const fs::path lexer_file = compile_selfhost_program(
        "lexer_file_for_compare",
        source_root() / "selfhost" / "src" / "aurex" / "selfhost" / "tool" / "lexer_file.ax"
    );
    std::vector<fs::path> corpus = {source_root() / "examples" / "hello.ax"};
    for (const fs::path& src : sorted_files(source_root() / "tests" / "positive", ".ax")) {
        corpus.push_back(src);
    }
    for (const fs::path& src : sorted_files(source_root() / "tests" / "negative", ".ax")) {
        corpus.push_back(src);
    }

    for (const fs::path& src : corpus) {
        const std::vector<std::string> stage0 = stage0_token_kinds(src);
        const std::vector<std::string> selfhost = lines(require_success(q(lexer_file) + " " + q(src)).output);
        EXPECT_EQ(stage0, selfhost) << src;
    }
}

TEST_F(AurexIntegrationTest, Stage1ModuleGraphAndSnapshots) {
    const fs::path selfhost = source_root() / "selfhost" / "src" / "aurex" / "selfhost";
    const fs::path stage1_src = selfhost / "bin" / "aurexc_stage1.ax";
    require_success(aurexc() + " " + selfhost_import_flags() + " --check " + q(stage1_src));

    const std::string modules = require_success(aurexc() + " " + selfhost_import_flags() + " --dump-modules " + q(stage1_src)).output;
    expect_contains_all(modules, {
        "aurex.selfhost.compiler.driver",
        "aurex.selfhost.compiler.ir.emit",
        "aurex.selfhost.compiler.air.model",
        "aurex.selfhost.compiler.air.bind",
        "aurex.selfhost.compiler.air.place",
        "aurex.selfhost.compiler.air.memory",
        "aurex.selfhost.compiler.air.flow",
        "aurex.selfhost.compiler.air.cfg",
        "aurex.selfhost.compiler.air.lower",
        "aurex.selfhost.compiler.air.text",
        "aurex.selfhost.compiler.air.verify",
        "aurex.selfhost.compiler.ir.expr",
        "aurex.selfhost.compiler.ir.cfg",
        "aurex.selfhost.compiler.ir.types",
        "aurex.selfhost.compiler.ir.names",
        "aurex.selfhost.compiler.ir.writer",
        "aurex.selfhost.sema.names",
        "aurex.selfhost.sema.calls",
        "aurex.selfhost.sema.items",
        "aurex.selfhost.sema.locals",
        "aurex.selfhost.sema.lvalues",
        "aurex.selfhost.sema.members",
        "aurex.selfhost.sema.resolve",
        "aurex.selfhost.sema.typing_types",
        "aurex.selfhost.sema.typing_lookup",
        "aurex.selfhost.sema.typing_infer",
        "aurex.selfhost.sema.annotate",
        "aurex.selfhost.sema.typing",
        "aurex.selfhost.sema.types",
    });

    const fs::path stage1_bin = compile_stage1();

    const std::string hello_tac = read_text(run_stage1(stage1_bin, source_root() / "examples" / "hello.ax", "hello.stage1.tac"));
    expect_contains_all(hello_tac, {
        "aurex_tac v0",
        "fn puts(s: *const u8) @puts linkage(extern_c) abi(c) -> i32",
        "fn main() @m0_hello_main linkage(internal)",
        "c_string c\"hello from Aurex M0\"",
        "ret %t",
    });
    EXPECT_EQ(count_lines_starting_with(hello_tac, "  ; air_ir lowering("), 0);

    const std::string seed_tac = read_text(run_stage1(stage1_bin, selfhost / "bin" / "aurexc_seed.ax", "aurexc_seed.stage1.tac"));
    expect_contains(seed_tac, "c_string c\"Aurex M0 selfhost seed\"");
    EXPECT_EQ(count_lines_starting_with(seed_tac, "  ; air_ir lowering("), 0);

    const std::string lang_tac = read_text(run_stage1(stage1_bin, selfhost / "smoke" / "stage1_lang.ax", "stage1_lang.stage1.tac"));
    expect_contains_all(lang_tac, {
        "edge break ^air",
        "edge continue ^air",
        "cfg ^block3 edge break",
        "cfg ^block4 edge continue",
    });
    EXPECT_EQ(count_lines_starting_with(lang_tac, "  ; air_ir lowering("), 0);

    const std::string flow_tac = read_text(run_stage1(stage1_bin, selfhost / "smoke" / "stage1_flow.ax", "stage1_flow.stage1.tac"));
    expect_contains_all(flow_tac, {
        "fn helper(limit: i32)",
        "linkage(internal)",
        "let one:",
        "var acc:",
        "assign %t",
        "while %t",
        "if %t",
        "block ^block",
        "air_ir v0",
        "air_edges v0",
        "air_cfg v0",
        "inst let",
        "inst assign",
        "slot @slot",
        "slot_addr",
        "load",
        "read(%v",
        "store",
        "effect(%v",
        "lvalue @lv",
        "lvalue(@lv",
        "bind(param",
        "bind(local",
        "bind(item",
        "type(",
        "sema_type(type_id",
        "sema_type(primitive",
        "edge while_true ^air",
        "edge while_false ^air0 -> ^air1",
        "edge loop_back ^air",
        "edge scope ^air0 -> ^air2",
        "edge if_true ^air1 -> ^air",
        "edge if_false ^air1 -> ^air",
        "block_param %v",
        "edge return ^air",
        "stmts(3+1)",
        "term branch %v",
        "term jump ^air0",
        "edge while %t",
        "false ^air1",
        "cfg ^air1 edge if",
        "edge loop_back ^entry",
        "edge if %t",
        "term ret %t",
    });
    EXPECT_EQ(count_lines_starting_with(flow_tac, "  ; air_ir lowering("), 0);

    const std::string expr_tac = read_text(run_stage1(stage1_bin, selfhost / "smoke" / "stage1_expr.ax", "stage1_expr.stage1.tac"));
    expect_contains_all(expr_tac, {
        "size_of <u8>",
        "align_of <u8>",
        "ptr_addr %t",
        "ptr_from_addr <",
        "cast <u8> %t",
        "ptr_cast <",
        "bit_cast <u32> %t",
        "struct Pair",
        ".value",
        "index ",
        "struct_literal",
        "field(",
        "target_type(",
        "name(",
        "sema_type(integer)",
        "sema_type(c_string)",
        "sema_type(item",
        "4]u8",
    });
    EXPECT_EQ(count_lines_starting_with(expr_tac, "  ; air_ir lowering("), 0);

    const std::string place_tac = read_text(run_stage1(stage1_bin, selfhost / "smoke" / "stage1_place.ax", "stage1_place.stage1.tac"));
    expect_contains_all(place_tac, {
        "assign %t",
        "lvalue @lv",
        "binding",
        "field",
        "index",
        "deref",
        "field_addr",
        "index_addr",
        "deref_addr",
        "load",
        "read(%v",
        "lvalue(@lv",
    });
    EXPECT_EQ(count_lines_starting_with(place_tac, "  ; air_ir lowering("), 0);

    const std::string items_tac = read_text(run_stage1(stage1_bin, selfhost / "smoke" / "stage1_items.ax", "stage1_items.stage1.tac"));
    expect_contains_all(items_tac, {
        "record NativeFile",
        "opaque",
        "enum Stage1Tag: u16",
        "zero = 0",
        "record Pair",
        "value: i32",
        "tag: aurex.selfhost.smoke.stage1_items.Stage1Tag",
        "const DEFAULT_VALUE: i32 = %t",
        "fn read_pair(pair:",
    });
    EXPECT_EQ(count_lines_starting_with(items_tac, "  ; air_ir lowering("), 0);

    const std::string ir_tac = read_text(run_stage1(stage1_bin, selfhost / "smoke" / "stage1_ir.ax", "stage1_ir.stage1.tac"));
    expect_contains_all(ir_tac, {
        "fn helper() @stage1_ir_helper linkage(export_c) abi(c) -> i32",
        "literal 40",
        "call %t",
        "add %t",
        "mul %t",
    });
    EXPECT_EQ(count_lines_starting_with(ir_tac, "  ; air_ir lowering("), 0);
}

TEST_F(AurexIntegrationTest, Stage1CompilerBundleAirGapBaseline) {
    const fs::path stage1_bin = compile_stage1();
    std::string command = q(stage1_bin);
    for (const fs::path& source : stage_compiler_sources()) {
        command += " " + q(source);
    }
    const fs::path bundle_tac = selfhost_bin_root() / "aurexc_stage1.bundle.tac";
    command += " " + q(bundle_tac);
    require_success(command);

    const std::string bundle = read_text(bundle_tac);
    expect_contains_all(bundle, {
        "aurex_tac v0",
        "fn compile_file(input_path: *const u8, output_path: *const u8)",
        "m0_aurex_selfhost_bin_aurexc_stage1_main",
    });

    EXPECT_EQ(count_lines_starting_with(bundle, "; selfhost_module "), 0)
        << "the selfhost compiler bundle should not fall back to parser or sema module placeholders";
    EXPECT_EQ(count_lines_starting_with(bundle, "  ; air_ir lowering("), 0);
    EXPECT_EQ(bundle.find("air_loop_control_pending"), std::string::npos);
}

} // namespace
