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

fs::path tmp_root() {
    return test_run_root() / "tmp";
}

fs::path aurexc_path() {
    return build_root() / "bin" / "aurexc";
}

fs::path samples_root() {
    return source_root() / "tests" / "samples";
}

fs::path positive_samples_root() {
    return samples_root() / "positive";
}

fs::path negative_samples_root() {
    return samples_root() / "negative";
}

fs::path imports_root() {
    return samples_root() / "imports";
}

fs::path golden_root() {
    return samples_root() / "golden";
}

fs::path positive_sample(const std::string_view area, const std::string_view filename) {
    return positive_samples_root() / std::string(area) / std::string(filename);
}

fs::path negative_sample(const std::string_view area, const std::string_view filename) {
    return negative_samples_root() / std::string(area) / std::string(filename);
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

std::vector<fs::path> sorted_files(const fs::path& dir, const std::string_view extension) {
    std::vector<fs::path> files;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(dir)) {
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

std::string sample_import_flags() {
    return "-I " + q(imports_root());
}

std::string tests_import_flags() {
    return sample_import_flags();
}

void AurexIntegrationTest::SetUpTestSuite() {
    if (!fs::exists(aurexc_path())) {
        throw std::runtime_error("missing aurexc binary: " + aurexc_path().string());
    }
}

void AurexIntegrationTest::SetUp() {
    current_test_run_root = make_test_run_root();
    fs::create_directories(test_bin_root());
    fs::create_directories(tmp_root());
}

} // namespace aurex::test
