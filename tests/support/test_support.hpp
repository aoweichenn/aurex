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
fs::path selfhost_bin_root();
fs::path tmp_root();
fs::path aurexc_path();

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

int count_lines_starting_with(std::string_view text, std::string_view prefix);
std::vector<fs::path> sorted_files(const fs::path& dir, std::string_view extension);
std::string stem(const fs::path& path);
std::string aurexc();
std::string selfhost_import_flags();
std::string tests_import_flags();

fs::path compile_selfhost_program(std::string_view name, const fs::path& source);
fs::path compile_stage1();
fs::path run_stage1(const fs::path& stage1_bin, const fs::path& source, std::string_view out_name);
std::vector<std::string> lines(const std::string& text);
std::vector<std::string> stage0_token_kinds(const fs::path& source);
std::vector<fs::path> stage_compiler_sources();

class AurexIntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite();
};

} // namespace aurex::test
