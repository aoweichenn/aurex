#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::test {

namespace fs = std::filesystem;

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

fs::path source_root();
fs::path build_root();
fs::path work_root();
fs::path test_bin_root();
fs::path tmp_root();
fs::path test_run_root();
fs::path aurexc_path();
fs::path samples_root();
fs::path positive_samples_root();
fs::path negative_samples_root();
fs::path imports_root();
fs::path golden_root();
fs::path positive_sample(std::string_view area, std::string_view filename);
fs::path negative_sample(std::string_view area, std::string_view filename);

std::string shell_quote(std::string_view value);
std::string q(const fs::path& path);
std::string q(std::string_view value);
CommandResult run_command(const std::string& command);
CommandResult require_success(const std::string& command);
CommandResult require_failure(const std::string& command);
std::string read_text(const fs::path& path);

void expect_contains(std::string_view text, std::string_view needle);
void expect_contains_all(std::string_view text, const std::vector<std::string_view>& needles);
void expect_not_contains(std::string_view text, std::string_view needle);

std::vector<fs::path> sorted_files(const fs::path& dir, std::string_view extension);
std::string stem(const fs::path& path);
std::string aurexc();
std::string sample_import_flags();
std::string tests_import_flags();

class AurexIntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite();
    void SetUp() override;
};

} // namespace aurex::test
