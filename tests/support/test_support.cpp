#include "support/test_support.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <unistd.h>
#include <sys/wait.h>

namespace aurex::test {

fs::path source_root() {
    return fs::path(AUREX_TEST_SOURCE_DIR);
}

fs::path build_root() {
    return fs::path(AUREX_TEST_BINARY_DIR);
}

fs::path work_root() {
    return build_root() / "gtest";
}

namespace {

std::atomic<std::uint64_t> test_run_counter {0};
fs::path current_test_run_root;

std::string sanitize_test_name(const std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (const char ch : name) {
        const bool ok = (ch >= 'a' && ch <= 'z') ||
                        (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9');
        result += ok ? ch : '_';
    }
    return result;
}

fs::path make_test_run_root() {
    const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name = "suite";
    if (info != nullptr) {
        test_name = std::string(info->test_suite_name()) + "_" + std::string(info->name());
    }
    const auto pid = static_cast<unsigned long long>(::getpid());
    const auto seq = static_cast<unsigned long long>(test_run_counter.fetch_add(1, std::memory_order_relaxed));
    return work_root() / (sanitize_test_name(test_name) + "_" + std::to_string(pid) + "_" + std::to_string(seq));
}

} // namespace

fs::path test_run_root() {
    if (current_test_run_root.empty()) {
        current_test_run_root = make_test_run_root();
    }
    return current_test_run_root;
}

fs::path test_bin_root() {
    return test_run_root() / "tests";
}

fs::path selfhost_bin_root() {
    return test_run_root() / "selfhost";
}

fs::path tmp_root() {
    return test_run_root() / "tmp";
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

namespace {

[[nodiscard]] int decode_status(const int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return status;
}

} // namespace

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

void AurexIntegrationTest::SetUpTestSuite() {
    if (!fs::exists(aurexc_path())) {
        throw std::runtime_error("missing aurexc binary: " + aurexc_path().string());
    }
}

void AurexIntegrationTest::SetUp() {
    current_test_run_root = make_test_run_root();
    fs::create_directories(test_bin_root());
    fs::create_directories(selfhost_bin_root());
    fs::create_directories(tmp_root());
}

} // namespace aurex::test
