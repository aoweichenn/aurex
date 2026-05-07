#include "support/test_support.hpp"

#include "aurex/driver/compiler.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include <unistd.h>
#include <sys/wait.h>

namespace aurex::test {

namespace {

std::atomic<std::uint64_t> test_run_counter {0};
fs::path current_test_run_root;

struct TestContext {
    fs::path source_root = fs::path(AUREX_TEST_SOURCE_DIR);
    fs::path build_root = fs::path(AUREX_TEST_BINARY_DIR);
    fs::path work_root = build_root / "gtest";
    fs::path aurexc_path = build_root / "bin" / "aurexc";
    fs::path samples_root = source_root / "tests" / "samples";
    fs::path positive_samples_root = samples_root / "positive";
    fs::path negative_samples_root = samples_root / "negative";
    fs::path imports_root = samples_root / "imports";
    fs::path golden_root = samples_root / "golden";
    std::mutex files_mutex;
    std::unordered_map<std::string, std::vector<fs::path>> sorted_files_cache;
    std::mutex compiler_cache_mutex;
    std::unordered_map<std::string, CommandResult> compiler_result_cache;
    std::unordered_map<std::string, fs::path> native_output_cache;
    std::uint64_t native_output_cache_counter = 0;
};

TestContext& test_context() {
    static TestContext context;
    return context;
}

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

fs::path source_root() {
    return test_context().source_root;
}

fs::path build_root() {
    return test_context().build_root;
}

fs::path work_root() {
    return test_context().work_root;
}

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
    return test_context().aurexc_path;
}

fs::path samples_root() {
    return test_context().samples_root;
}

fs::path positive_samples_root() {
    return test_context().positive_samples_root;
}

fs::path negative_samples_root() {
    return test_context().negative_samples_root;
}

fs::path imports_root() {
    return test_context().imports_root;
}

fs::path golden_root() {
    return test_context().golden_root;
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

class CompilerOutputCapture final {
public:
    CompilerOutputCapture() {
        std::string path_template =
            (fs::temp_directory_path() / "aurex_compiler_capture_XXXXXX").string();
        capture_fd_ = ::mkstemp(path_template.data());
        if (capture_fd_ < 0) {
            throw std::runtime_error("failed to create compiler output capture file: " + std::string(std::strerror(errno)));
        }
        capture_path_ = path_template;

        stdout_fd_ = ::dup(STDOUT_FILENO);
        stderr_fd_ = ::dup(STDERR_FILENO);
        if (stdout_fd_ < 0 || stderr_fd_ < 0) {
            close_open_files();
            throw std::runtime_error("failed to duplicate compiler output file descriptors");
        }

        std::cout.flush();
        std::cerr.flush();
        std::fflush(stdout);
        std::fflush(stderr);
        if (::dup2(capture_fd_, STDOUT_FILENO) < 0 || ::dup2(capture_fd_, STDERR_FILENO) < 0) {
            restore_file_descriptors();
            close_open_files();
            throw std::runtime_error("failed to redirect compiler output file descriptors");
        }

        original_cout_ = std::cout.rdbuf(cxx_output_.rdbuf());
        original_cerr_ = std::cerr.rdbuf(cxx_output_.rdbuf());
        active_ = true;
    }

    CompilerOutputCapture(const CompilerOutputCapture&) = delete;
    CompilerOutputCapture& operator=(const CompilerOutputCapture&) = delete;

    ~CompilerOutputCapture() {
        restore();
        close_open_files();
    }

    [[nodiscard]] std::string finish() {
        restore();
        std::string output = cxx_output_.str();
        if (capture_fd_ >= 0) {
            static_cast<void>(::lseek(capture_fd_, 0, SEEK_SET));
            std::array<char, 4096> buffer {};
            ssize_t count = 0;
            while ((count = ::read(capture_fd_, buffer.data(), buffer.size())) > 0) {
                output.append(buffer.data(), static_cast<std::size_t>(count));
            }
        }
        return output;
    }

private:
    void restore() {
        if (!active_) {
            return;
        }
        std::cout.flush();
        std::cerr.flush();
        std::cout.rdbuf(original_cout_);
        std::cerr.rdbuf(original_cerr_);
        std::fflush(stdout);
        std::fflush(stderr);
        restore_file_descriptors();
        active_ = false;
    }

    void restore_file_descriptors() {
        if (stdout_fd_ >= 0) {
            static_cast<void>(::dup2(stdout_fd_, STDOUT_FILENO));
            ::close(stdout_fd_);
            stdout_fd_ = -1;
        }
        if (stderr_fd_ >= 0) {
            static_cast<void>(::dup2(stderr_fd_, STDERR_FILENO));
            ::close(stderr_fd_);
            stderr_fd_ = -1;
        }
    }

    void close_open_files() {
        restore_file_descriptors();
        if (capture_fd_ >= 0) {
            ::close(capture_fd_);
            capture_fd_ = -1;
        }
        if (!capture_path_.empty()) {
            std::error_code error;
            fs::remove(capture_path_, error);
            capture_path_.clear();
        }
    }

    std::ostringstream cxx_output_;
    std::streambuf* original_cout_ = nullptr;
    std::streambuf* original_cerr_ = nullptr;
    int capture_fd_ = -1;
    int stdout_fd_ = -1;
    int stderr_fd_ = -1;
    fs::path capture_path_;
    bool active_ = false;
};

[[nodiscard]] std::optional<std::vector<std::string>> split_shell_words(const std::string_view command) {
    std::vector<std::string> words;
    std::string current;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool saw_word = false;

    for (base::usize i = 0; i < command.size(); ++i) {
        const char ch = command[i];
        if (in_single_quote) {
            if (ch == '\'') {
                in_single_quote = false;
            } else {
                current.push_back(ch);
                saw_word = true;
            }
            continue;
        }
        if (in_double_quote) {
            if (ch == '"') {
                in_double_quote = false;
            } else if (ch == '\\' && i + 1 < command.size()) {
                current.push_back(command[++i]);
                saw_word = true;
            } else {
                current.push_back(ch);
                saw_word = true;
            }
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (saw_word) {
                words.push_back(std::move(current));
                current.clear();
                saw_word = false;
            }
            continue;
        }
        if (ch == '\'') {
            in_single_quote = true;
            saw_word = true;
            continue;
        }
        if (ch == '"') {
            in_double_quote = true;
            saw_word = true;
            continue;
        }
        if (ch == '\\' && i + 1 < command.size()) {
            current.push_back(command[++i]);
            saw_word = true;
            continue;
        }
        if (std::string_view("|&;<>()$`").find(ch) != std::string_view::npos) {
            return std::nullopt;
        }
        current.push_back(ch);
        saw_word = true;
    }

    if (in_single_quote || in_double_quote) {
        return std::nullopt;
    }
    if (saw_word) {
        words.push_back(std::move(current));
    }
    return words;
}

[[nodiscard]] bool parse_emit_kind(const std::string_view arg, driver::EmitKind& emit_kind) noexcept {
    if (arg == "--dump-tokens" || arg == "--emit=tokens") {
        emit_kind = driver::EmitKind::tokens;
    } else if (arg == "--dump-ast" || arg == "--emit=ast") {
        emit_kind = driver::EmitKind::ast;
    } else if (arg == "--dump-modules" || arg == "--emit=modules") {
        emit_kind = driver::EmitKind::modules;
    } else if (arg == "--dump-checked" || arg == "--emit=checked") {
        emit_kind = driver::EmitKind::checked;
    } else if (arg == "--dump-ir" || arg == "--emit=ir") {
        emit_kind = driver::EmitKind::ir;
    } else if (arg == "--dump-llvm-ir" || arg == "--emit=llvm-ir") {
        emit_kind = driver::EmitKind::llvm_ir;
    } else if (arg == "--check" || arg == "--emit=check") {
        emit_kind = driver::EmitKind::check;
    } else if (arg == "--emit=asm") {
        emit_kind = driver::EmitKind::assembly;
    } else if (arg == "--emit=obj" || arg == "--emit=object") {
        emit_kind = driver::EmitKind::object;
    } else if (arg == "--emit=exe") {
        emit_kind = driver::EmitKind::executable;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] bool parse_optimization_level(
    const std::string_view level,
    ir::OptimizationLevel& optimization_level
) noexcept {
    if (level == "0" || level == "O0") {
        optimization_level = ir::OptimizationLevel::none;
    } else if (level == "1" || level == "O1") {
        optimization_level = ir::OptimizationLevel::basic;
    } else if (level == "2" || level == "O2") {
        optimization_level = ir::OptimizationLevel::standard;
    } else if (level == "3" || level == "O3") {
        optimization_level = ir::OptimizationLevel::aggressive;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<CommandResult> try_run_aurexc_command(const std::string& command) {
    const std::optional<std::vector<std::string>> parsed = split_shell_words(command);
    if (!parsed || parsed->empty() || parsed->front() != aurexc_path().string()) {
        return std::nullopt;
    }

    driver::CompilerInvocation invocation;
    invocation.tool_path = aurexc_path();

    for (base::usize i = 1; i < parsed->size(); ++i) {
        const std::string& arg = (*parsed)[i];
        if (parse_emit_kind(arg, invocation.emit_kind)) {
            continue;
        }
        if (arg == "--help" || arg == "-h" || arg == "--version") {
            return std::nullopt;
        }
        if (arg == "-o") {
            if (++i >= parsed->size()) {
                return std::nullopt;
            }
            invocation.output_path = (*parsed)[i];
        } else if (arg == "-I") {
            if (++i >= parsed->size()) {
                return std::nullopt;
            }
            invocation.import_paths.push_back((*parsed)[i]);
        } else if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
            invocation.import_paths.push_back(arg.substr(2));
        } else if (arg == "--clang") {
            if (++i >= parsed->size()) {
                return std::nullopt;
            }
            invocation.clang_path = (*parsed)[i];
        } else if (arg == "--clang-arg") {
            if (++i >= parsed->size()) {
                return std::nullopt;
            }
            invocation.clang_args.push_back((*parsed)[i]);
        } else if (arg == "--opt-level" || arg == "-O") {
            if (++i >= parsed->size() ||
                !parse_optimization_level((*parsed)[i], invocation.optimization_level)) {
                return std::nullopt;
            }
        } else if ((arg == "-O0" || arg == "-O1" || arg == "-O2" || arg == "-O3") && arg.size() == 3) {
            invocation.optimization_level = static_cast<ir::OptimizationLevel>(arg[2] - '0');
        } else if (arg == "--stdlib") {
            if (++i >= parsed->size()) {
                return std::nullopt;
            }
            invocation.standard_library_path = (*parsed)[i];
        } else if (arg == "--std-backend") {
            if (++i >= parsed->size()) {
                return std::nullopt;
            }
            const std::string& backend = (*parsed)[i];
            if (backend == "host-c") {
                invocation.standard_library_backend = driver::StandardLibraryBackend::host_c;
            } else if (backend == "none") {
                invocation.standard_library_backend = driver::StandardLibraryBackend::none;
            } else {
                return std::nullopt;
            }
        } else if (arg == "--no-stdlib") {
            invocation.use_standard_library = false;
        } else if (!arg.empty() && arg.front() == '-') {
            return std::nullopt;
        } else {
            if (!invocation.input_path.empty()) {
                return std::nullopt;
            }
            invocation.input_path = arg;
        }
    }

    if (invocation.input_path.empty()) {
        return std::nullopt;
    }
    return run_compiler(invocation);
}

[[nodiscard]] bool is_native_emit_kind(const driver::EmitKind emit_kind) noexcept {
    return emit_kind == driver::EmitKind::assembly ||
           emit_kind == driver::EmitKind::object ||
           emit_kind == driver::EmitKind::executable;
}

[[nodiscard]] fs::path canonical_or_absolute(const fs::path& path) {
    std::error_code canonical_error;
    fs::path canonical = fs::weakly_canonical(path, canonical_error);
    if (!canonical_error) {
        return canonical;
    }

    std::error_code absolute_error;
    fs::path absolute = fs::absolute(path, absolute_error);
    return absolute_error ? path : absolute;
}

[[nodiscard]] bool append_file_identity(std::string& key, const fs::path& path) {
    const fs::path resolved = canonical_or_absolute(path);
    key += resolved.string();
    key.push_back('\n');

    std::error_code size_error;
    const std::uintmax_t size = fs::file_size(resolved, size_error);
    std::error_code modified_error;
    const auto modified = fs::last_write_time(resolved, modified_error);
    if (size_error || modified_error) {
        return false;
    }

    key += "size=";
    key += std::to_string(size);
    key += "\nmtime=";
    key += std::to_string(static_cast<long long>(modified.time_since_epoch().count()));
    key.push_back('\n');
    return true;
}

void append_path_list_identity(std::string& key, const std::vector<fs::path>& paths) {
    key += "paths=";
    key += std::to_string(paths.size());
    key.push_back('\n');
    for (const fs::path& path : paths) {
        key += canonical_or_absolute(path).string();
        key.push_back('\n');
    }
}

[[nodiscard]] std::optional<std::string> compiler_cache_key(const driver::CompilerInvocation& invocation) {
    std::string key;
    key += "emit=";
    key += std::to_string(static_cast<int>(invocation.emit_kind));
    key.push_back('\n');
    key += "input=";
    if (!append_file_identity(key, invocation.input_path)) {
        return std::nullopt;
    }
    append_path_list_identity(key, invocation.import_paths);
    key += "tool=";
    key += canonical_or_absolute(invocation.tool_path).string();
    key.push_back('\n');
    key += "stdlib=";
    key += canonical_or_absolute(invocation.standard_library_path).string();
    key.push_back('\n');
    key += "stdlib_env=";
    if (const char* env = std::getenv("AUREX_STDLIB"); env != nullptr) {
        key += env;
    }
    key.push_back('\n');
    key += "cwd=";
    std::error_code cwd_error;
    key += fs::current_path(cwd_error).string();
    key.push_back('\n');
    key += "clang=";
    key += invocation.clang_path;
    key.push_back('\n');
    key += "clang_args=";
    key += std::to_string(invocation.clang_args.size());
    key.push_back('\n');
    for (const std::string& arg : invocation.clang_args) {
        key += arg;
        key.push_back('\n');
    }
    key += "opt=";
    key += std::to_string(static_cast<int>(invocation.optimization_level));
    key.push_back('\n');
    key += "std_backend=";
    key += std::to_string(static_cast<int>(invocation.standard_library_backend));
    key.push_back('\n');
    key += invocation.use_standard_library ? "use_std=1\n" : "use_std=0\n";
    return key;
}

[[nodiscard]] std::optional<CommandResult> try_restore_cached_compiler_result(
    const driver::CompilerInvocation& invocation,
    const std::string& cache_key
) {
    if (is_native_emit_kind(invocation.emit_kind)) {
        return std::nullopt;
    }

    TestContext& context = test_context();
    std::lock_guard lock(context.compiler_cache_mutex);
    if (const auto found = context.compiler_result_cache.find(cache_key); found != context.compiler_result_cache.end()) {
        return found->second;
    }
    return std::nullopt;
}

[[nodiscard]] bool copy_file_to_output(const fs::path& source, const fs::path& destination, const driver::EmitKind emit_kind) {
    if (destination.empty()) {
        return false;
    }
    std::error_code error;
    const fs::path parent = destination.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, error);
        if (error) {
            return false;
        }
    }
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
    if (error) {
        return false;
    }
    if (emit_kind == driver::EmitKind::executable) {
        fs::permissions(
            destination,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add,
            error
        );
        if (error) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<CommandResult> try_restore_cached_native_output(
    const driver::CompilerInvocation& invocation,
    const std::string& cache_key
) {
    if (!is_native_emit_kind(invocation.emit_kind) || invocation.output_path.empty()) {
        return std::nullopt;
    }

    fs::path cached_output;
    {
        TestContext& context = test_context();
        std::lock_guard lock(context.compiler_cache_mutex);
        if (const auto found = context.native_output_cache.find(cache_key); found != context.native_output_cache.end()) {
            cached_output = found->second;
        }
    }
    if (cached_output.empty() ||
        !copy_file_to_output(cached_output, invocation.output_path, invocation.emit_kind)) {
        return std::nullopt;
    }
    return CommandResult {0, ""};
}

void remember_compiler_result(
    const driver::CompilerInvocation& invocation,
    const std::string& cache_key,
    const CommandResult& result
) {
    if (is_native_emit_kind(invocation.emit_kind)) {
        return;
    }

    TestContext& context = test_context();
    std::lock_guard lock(context.compiler_cache_mutex);
    context.compiler_result_cache.emplace(cache_key, result);
}

void remember_native_output(
    const driver::CompilerInvocation& invocation,
    const std::string& cache_key,
    const CommandResult& result
) {
    if (!is_native_emit_kind(invocation.emit_kind) ||
        invocation.output_path.empty() ||
        result.exit_code != 0 ||
        !fs::exists(invocation.output_path)) {
        return;
    }

    TestContext& context = test_context();
    fs::path cache_path;
    {
        std::lock_guard lock(context.compiler_cache_mutex);
        if (context.native_output_cache.find(cache_key) != context.native_output_cache.end()) {
            return;
        }
        const fs::path cache_dir = work_root() / "native-cache";
        cache_path = cache_dir / (
            "artifact_" +
            std::to_string(context.native_output_cache_counter++) +
            invocation.output_path.extension().string()
        );
    }

    std::error_code error;
    fs::create_directories(cache_path.parent_path(), error);
    if (error ||
        !copy_file_to_output(invocation.output_path, cache_path, invocation.emit_kind)) {
        return;
    }

    std::lock_guard lock(context.compiler_cache_mutex);
    context.native_output_cache.emplace(cache_key, cache_path);
}

} // namespace

CommandResult run_command(const std::string& command) {
    if (std::optional<CommandResult> fast_result = try_run_aurexc_command(command)) {
        return *fast_result;
    }

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

CommandResult run_compiler(const driver::CompilerInvocation& invocation) {
    const std::optional<std::string> cache_key = compiler_cache_key(invocation);
    if (cache_key) {
        if (std::optional<CommandResult> cached_result =
                try_restore_cached_compiler_result(invocation, *cache_key)) {
            return *cached_result;
        }
        if (std::optional<CommandResult> cached_native =
                try_restore_cached_native_output(invocation, *cache_key)) {
            return *cached_native;
        }
    }

    CompilerOutputCapture output;
    driver::Compiler compiler;
    auto result = compiler.run(invocation);
    if (!result) {
        std::cerr << "aurexc: " << result.error().message << "\n";
    }
    CommandResult command_result {result ? 0 : 1, output.finish()};
    if (cache_key) {
        remember_compiler_result(invocation, *cache_key, command_result);
        remember_native_output(invocation, *cache_key, command_result);
    }
    return command_result;
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

CommandResult require_compiler_success(const driver::CompilerInvocation& invocation) {
    CommandResult result = run_compiler(invocation);
    if (result.exit_code != 0) {
        throw std::runtime_error(
            "compiler invocation failed with exit code " + std::to_string(result.exit_code) + "\n" +
            invocation.input_path.string() + "\n" + result.output
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

CommandResult require_compiler_failure(const driver::CompilerInvocation& invocation) {
    CommandResult result = run_compiler(invocation);
    if (result.exit_code == 0) {
        throw std::runtime_error("compiler invocation unexpectedly succeeded\n" + invocation.input_path.string() + "\n" + result.output);
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
    TestContext& context = test_context();
    const std::string cache_key = dir.string() + "\n" + std::string(extension);
    {
        std::lock_guard lock(context.files_mutex);
        if (const auto found = context.sorted_files_cache.find(cache_key); found != context.sorted_files_cache.end()) {
            return found->second;
        }
    }

    std::vector<fs::path> files;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    {
        std::lock_guard lock(context.files_mutex);
        context.sorted_files_cache.emplace(cache_key, files);
    }
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
