#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <benchmark/benchmark.h>

namespace {

constexpr std::int64_t FRONTEND_COMPARE_ITEM_COUNT = 96;
constexpr std::string_view FRONTEND_COMPARE_AUREXC_ENV = "AUREX_COMPARE_AUREXC";
constexpr std::string_view FRONTEND_COMPARE_CLANGXX_ENV = "AUREX_COMPARE_CLANGXX";
constexpr std::string_view FRONTEND_COMPARE_GXX_ENV = "AUREX_COMPARE_GXX";
constexpr std::string_view FRONTEND_COMPARE_RUSTC_ENV = "AUREX_COMPARE_RUSTC";
constexpr std::string_view FRONTEND_COMPARE_AUREXC_DEFAULT = "aurexc";
constexpr std::string_view FRONTEND_COMPARE_CLANGXX_DEFAULT = "clang++";
constexpr std::string_view FRONTEND_COMPARE_GXX_DEFAULT = "g++";
constexpr std::string_view FRONTEND_COMPARE_RUSTC_DEFAULT = "rustc";
constexpr std::string_view FRONTEND_COMPARE_PATH_ENV = "PATH";
constexpr char FRONTEND_COMPARE_PATH_SEPARATOR = ':';
constexpr std::string_view FRONTEND_COMPARE_DEV_NULL = "/dev/null";
constexpr int FRONTEND_COMPARE_COMMAND_NOT_FOUND_EXIT = 127;
constexpr int FRONTEND_COMPARE_COMMAND_NOT_EXECUTABLE_EXIT = 126;

enum class FrontendCompiler {
    aurex,
    clangxx,
    gxx,
    rustc,
};

enum class FrontendWorkload {
    lookup,
    generics,
};

struct BenchSpec final {
    FrontendCompiler compiler;
    FrontendWorkload workload;
    std::int64_t item_count;
};

struct ProcessResult final {
    bool ok = false;
    std::string message;
};

[[nodiscard]] const char* compiler_label(const FrontendCompiler compiler) noexcept
{
    switch (compiler) {
        case FrontendCompiler::aurex:
            return "aurex";
        case FrontendCompiler::clangxx:
            return "clang++";
        case FrontendCompiler::gxx:
            return "g++";
        case FrontendCompiler::rustc:
            return "rustc";
    }
    return "unknown";
}

[[nodiscard]] std::string make_aurex_lookup_source(const std::int64_t item_count)
{
    std::string source;
    source += "module bench.compare.lookup;\n\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        const std::string suffix = std::to_string(index);
        source += "struct Rec";
        source += suffix;
        source += " {\n"
                  "    left: i32;\n"
                  "    right: i32;\n"
                  "}\n\n"
                  "fn helper";
        source += suffix;
        source += "(value: i32) -> i32 {\n"
                  "    return value + ";
        source += suffix;
        source += ";\n"
                  "}\n\n"
                  "fn use";
        source += suffix;
        source += "(seed: i32) -> i32 {\n"
                  "    let record: Rec";
        source += suffix;
        source += " = Rec";
        source += suffix;
        source += " { left: seed, right: seed + 1 };\n"
                  "    return helper";
        source += suffix;
        source += "(record.left) + record.right;\n"
                  "}\n\n";
    }
    source += "fn main() -> i32 {\n"
              "    var total: i32 = 0;\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        source += "    total = total + use";
        source += std::to_string(index);
        source += "(";
        source += std::to_string(index);
        source += ");\n";
    }
    source += "    return total;\n"
              "}\n";
    return source;
}

[[nodiscard]] std::string make_aurex_generic_source(const std::int64_t item_count)
{
    std::string source;
    source += "module bench.compare.generics;\n\n"
              "struct Box[T] {\n"
              "    value: T;\n"
              "}\n\n"
              "fn id[T](value: T) -> T {\n"
              "    return value;\n"
              "}\n\n"
              "fn make_box[T](value: T) -> Box[T] {\n"
              "    return Box[T] { value: value };\n"
              "}\n\n"
              "fn unwrap_box[T](box: Box[T]) -> T {\n"
              "    return box.value;\n"
              "}\n\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        const std::string suffix = std::to_string(index);
        source += "struct Payload";
        source += suffix;
        source += " {\n"
                  "    value: i32;\n"
                  "}\n\n"
                  "fn use_payload";
        source += suffix;
        source += "(seed: i32) -> i32 {\n"
                  "    let payload: Payload";
        source += suffix;
        source += " = Payload";
        source += suffix;
        source += " { value: seed };\n"
                  "    let boxed: Box[Payload";
        source += suffix;
        source += "] = make_box[Payload";
        source += suffix;
        source += "](payload);\n"
                  "    let unwrapped: Payload";
        source += suffix;
        source += " = unwrap_box[Payload";
        source += suffix;
        source += "](boxed);\n"
                  "    return id[i32](unwrapped.value);\n"
                  "}\n\n";
    }
    source += "fn main() -> i32 {\n"
              "    var total: i32 = 0;\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        source += "    total = total + use_payload";
        source += std::to_string(index);
        source += "(";
        source += std::to_string(index);
        source += ");\n";
    }
    source += "    return total;\n"
              "}\n";
    return source;
}

[[nodiscard]] std::string make_cpp_lookup_source(const std::int64_t item_count)
{
    std::string source;
    source += "#include <cstdint>\n\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        const std::string suffix = std::to_string(index);
        source += "struct Rec";
        source += suffix;
        source += " {\n"
                  "    std::int32_t left;\n"
                  "    std::int32_t right;\n"
                  "};\n\n"
                  "std::int32_t helper";
        source += suffix;
        source += "(std::int32_t value) {\n"
                  "    return value + ";
        source += suffix;
        source += ";\n"
                  "}\n\n"
                  "std::int32_t use";
        source += suffix;
        source += "(std::int32_t seed) {\n"
                  "    Rec";
        source += suffix;
        source += " record {seed, seed + 1};\n"
                  "    return helper";
        source += suffix;
        source += "(record.left) + record.right;\n"
                  "}\n\n";
    }
    source += "std::int32_t entry() {\n"
              "    std::int32_t total = 0;\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        source += "    total = total + use";
        source += std::to_string(index);
        source += "(";
        source += std::to_string(index);
        source += ");\n";
    }
    source += "    return total;\n"
              "}\n";
    return source;
}

[[nodiscard]] std::string make_cpp_generic_source(const std::int64_t item_count)
{
    std::string source;
    source += "#include <cstdint>\n\n"
              "template <class T>\n"
              "struct Box {\n"
              "    T value;\n"
              "};\n\n"
              "template <class T>\n"
              "T id(T value) {\n"
              "    return value;\n"
              "}\n\n"
              "template <class T>\n"
              "Box<T> make_box(T value) {\n"
              "    return Box<T> {value};\n"
              "}\n\n"
              "template <class T>\n"
              "T unwrap_box(Box<T> box) {\n"
              "    return box.value;\n"
              "}\n\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        const std::string suffix = std::to_string(index);
        source += "struct Payload";
        source += suffix;
        source += " {\n"
                  "    std::int32_t value;\n"
                  "};\n\n"
                  "std::int32_t use_payload";
        source += suffix;
        source += "(std::int32_t seed) {\n"
                  "    Payload";
        source += suffix;
        source += " payload {seed};\n"
                  "    Box<Payload";
        source += suffix;
        source += "> boxed = make_box<Payload";
        source += suffix;
        source += ">(payload);\n"
                  "    Payload";
        source += suffix;
        source += " unwrapped = unwrap_box<Payload";
        source += suffix;
        source += ">(boxed);\n"
                  "    return id<std::int32_t>(unwrapped.value);\n"
                  "}\n\n";
    }
    source += "std::int32_t entry() {\n"
              "    std::int32_t total = 0;\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        source += "    total = total + use_payload";
        source += std::to_string(index);
        source += "(";
        source += std::to_string(index);
        source += ");\n";
    }
    source += "    return total;\n"
              "}\n";
    return source;
}

[[nodiscard]] std::string make_rust_lookup_source(const std::int64_t item_count)
{
    std::string source;
    for (std::int64_t index = 0; index < item_count; ++index) {
        const std::string suffix = std::to_string(index);
        source += "struct Rec";
        source += suffix;
        source += " {\n"
                  "    left: i32,\n"
                  "    right: i32,\n"
                  "}\n\n"
                  "fn helper";
        source += suffix;
        source += "(value: i32) -> i32 {\n"
                  "    value + ";
        source += suffix;
        source += "\n"
                  "}\n\n"
                  "fn use";
        source += suffix;
        source += "(seed: i32) -> i32 {\n"
                  "    let record = Rec";
        source += suffix;
        source += " { left: seed, right: seed + 1 };\n"
                  "    helper";
        source += suffix;
        source += "(record.left) + record.right\n"
                  "}\n\n";
    }
    source += "pub fn entry() -> i32 {\n"
              "    let mut total: i32 = 0;\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        source += "    total = total + use";
        source += std::to_string(index);
        source += "(";
        source += std::to_string(index);
        source += ");\n";
    }
    source += "    total\n"
              "}\n";
    return source;
}

[[nodiscard]] std::string make_rust_generic_source(const std::int64_t item_count)
{
    std::string source;
    source += "struct AurexBox<T> {\n"
              "    value: T,\n"
              "}\n\n"
              "fn id<T>(value: T) -> T {\n"
              "    value\n"
              "}\n\n"
              "fn make_box<T>(value: T) -> AurexBox<T> {\n"
              "    AurexBox { value }\n"
              "}\n\n"
              "fn unwrap_box<T>(boxed: AurexBox<T>) -> T {\n"
              "    boxed.value\n"
              "}\n\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        const std::string suffix = std::to_string(index);
        source += "struct Payload";
        source += suffix;
        source += " {\n"
                  "    value: i32,\n"
                  "}\n\n"
                  "fn use_payload";
        source += suffix;
        source += "(seed: i32) -> i32 {\n"
                  "    let payload = Payload";
        source += suffix;
        source += " { value: seed };\n"
                  "    let boxed = make_box::<Payload";
        source += suffix;
        source += ">(payload);\n"
                  "    let unwrapped = unwrap_box::<Payload";
        source += suffix;
        source += ">(boxed);\n"
                  "    id::<i32>(unwrapped.value)\n"
                  "}\n\n";
    }
    source += "pub fn entry() -> i32 {\n"
              "    let mut total: i32 = 0;\n";
    for (std::int64_t index = 0; index < item_count; ++index) {
        source += "    total = total + use_payload";
        source += std::to_string(index);
        source += "(";
        source += std::to_string(index);
        source += ");\n";
    }
    source += "    total\n"
              "}\n";
    return source;
}

void write_file(const std::filesystem::path& path, const std::string_view contents)
{
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open benchmark source file: " + path.string());
    }
    out << contents;
    if (!out) {
        throw std::runtime_error("failed to write benchmark source file: " + path.string());
    }
}

class SourceCorpus final {
public:
    SourceCorpus()
    {
        this->root_ = std::filesystem::temp_directory_path()
            / ("aurex_frontend_compare_" + std::to_string(static_cast<long long>(getpid())));
        std::error_code remove_error;
        std::filesystem::remove_all(this->root_, remove_error);

        std::error_code create_error;
        std::filesystem::create_directories(this->root_, create_error);
        if (create_error) {
            throw std::runtime_error("failed to create benchmark source directory: " + create_error.message());
        }

        this->aurex_lookup_source_ = this->root_ / "lookup.ax";
        this->aurex_generic_source_ = this->root_ / "generics.ax";
        this->cpp_lookup_source_ = this->root_ / "lookup.cpp";
        this->cpp_generic_source_ = this->root_ / "generics.cpp";
        this->rust_lookup_source_ = this->root_ / "lookup.rs";
        this->rust_generic_source_ = this->root_ / "generics.rs";
        this->rust_lookup_metadata_ = this->root_ / "lookup.rmeta";
        this->rust_generic_metadata_ = this->root_ / "generics.rmeta";

        write_file(this->aurex_lookup_source_, make_aurex_lookup_source(FRONTEND_COMPARE_ITEM_COUNT));
        write_file(this->aurex_generic_source_, make_aurex_generic_source(FRONTEND_COMPARE_ITEM_COUNT));
        write_file(this->cpp_lookup_source_, make_cpp_lookup_source(FRONTEND_COMPARE_ITEM_COUNT));
        write_file(this->cpp_generic_source_, make_cpp_generic_source(FRONTEND_COMPARE_ITEM_COUNT));
        write_file(this->rust_lookup_source_, make_rust_lookup_source(FRONTEND_COMPARE_ITEM_COUNT));
        write_file(this->rust_generic_source_, make_rust_generic_source(FRONTEND_COMPARE_ITEM_COUNT));
    }

    SourceCorpus(const SourceCorpus&) = delete;
    SourceCorpus& operator=(const SourceCorpus&) = delete;

    ~SourceCorpus()
    {
        std::error_code error;
        std::filesystem::remove_all(this->root_, error);
    }

    [[nodiscard]] const std::filesystem::path& source_path(
        const FrontendCompiler compiler, const FrontendWorkload workload) const noexcept
    {
        if (compiler == FrontendCompiler::aurex) {
            return workload == FrontendWorkload::lookup ? this->aurex_lookup_source_ : this->aurex_generic_source_;
        }
        if (compiler == FrontendCompiler::rustc) {
            return workload == FrontendWorkload::lookup ? this->rust_lookup_source_ : this->rust_generic_source_;
        }
        return workload == FrontendWorkload::lookup ? this->cpp_lookup_source_ : this->cpp_generic_source_;
    }

    [[nodiscard]] const std::filesystem::path& rust_metadata_path(const FrontendWorkload workload) const noexcept
    {
        return workload == FrontendWorkload::lookup ? this->rust_lookup_metadata_ : this->rust_generic_metadata_;
    }

private:
    std::filesystem::path root_;
    std::filesystem::path aurex_lookup_source_;
    std::filesystem::path aurex_generic_source_;
    std::filesystem::path cpp_lookup_source_;
    std::filesystem::path cpp_generic_source_;
    std::filesystem::path rust_lookup_source_;
    std::filesystem::path rust_generic_source_;
    std::filesystem::path rust_lookup_metadata_;
    std::filesystem::path rust_generic_metadata_;
};

[[nodiscard]] const SourceCorpus& source_corpus()
{
    static const SourceCorpus corpus;
    return corpus;
}

[[nodiscard]] std::string env_or_default(const std::string_view env_name, const std::string_view fallback)
{
    const char* value = std::getenv(std::string(env_name).c_str());
    if (value == nullptr || std::string_view(value).empty()) {
        return std::string(fallback);
    }
    return std::string(value);
}

[[nodiscard]] bool is_executable_file(const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        return false;
    }
    return access(path.c_str(), X_OK) == 0;
}

[[nodiscard]] bool has_path_separator(const std::string_view value) noexcept
{
    return value.find('/') != std::string_view::npos;
}

[[nodiscard]] std::optional<std::filesystem::path> find_executable(const std::string& requested)
{
    if (requested.empty()) {
        return std::nullopt;
    }
    if (has_path_separator(requested)) {
        std::filesystem::path path(requested);
        if (is_executable_file(path)) {
            return path;
        }
        return std::nullopt;
    }

    const char* path_env = std::getenv(std::string(FRONTEND_COMPARE_PATH_ENV).c_str());
    if (path_env == nullptr) {
        return std::nullopt;
    }
    std::string_view path_list(path_env);
    std::size_t begin = 0;
    while (begin <= path_list.size()) {
        const std::size_t end = path_list.find(FRONTEND_COMPARE_PATH_SEPARATOR, begin);
        const std::string_view entry =
            path_list.substr(begin, end == std::string_view::npos ? std::string_view::npos : end - begin);
        const std::filesystem::path directory =
            entry.empty() ? std::filesystem::path(".") : std::filesystem::path(entry);
        const std::filesystem::path candidate = directory / requested;
        if (is_executable_file(candidate)) {
            return candidate;
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return std::nullopt;
}

[[nodiscard]] std::string requested_tool(const FrontendCompiler compiler)
{
    switch (compiler) {
        case FrontendCompiler::aurex:
            return env_or_default(FRONTEND_COMPARE_AUREXC_ENV, FRONTEND_COMPARE_AUREXC_DEFAULT);
        case FrontendCompiler::clangxx:
            return env_or_default(FRONTEND_COMPARE_CLANGXX_ENV, FRONTEND_COMPARE_CLANGXX_DEFAULT);
        case FrontendCompiler::gxx:
            return env_or_default(FRONTEND_COMPARE_GXX_ENV, FRONTEND_COMPARE_GXX_DEFAULT);
        case FrontendCompiler::rustc:
            return env_or_default(FRONTEND_COMPARE_RUSTC_ENV, FRONTEND_COMPARE_RUSTC_DEFAULT);
    }
    return {};
}

[[nodiscard]] std::optional<std::filesystem::path> find_tool(const FrontendCompiler compiler)
{
    return find_executable(requested_tool(compiler));
}

[[nodiscard]] std::vector<std::string> build_command(
    const BenchSpec spec, const SourceCorpus& corpus, const std::filesystem::path& tool)
{
    const std::filesystem::path source = corpus.source_path(spec.compiler, spec.workload);
    switch (spec.compiler) {
        case FrontendCompiler::aurex:
            return {
                tool.string(),
                "--check",
                source.string(),
            };
        case FrontendCompiler::clangxx:
        case FrontendCompiler::gxx:
            return {
                tool.string(),
                "-std=c++20",
                "-fsyntax-only",
                "-w",
                source.string(),
            };
        case FrontendCompiler::rustc:
            return {
                tool.string(),
                "--edition=2021",
                "-Awarnings",
                "--crate-type=lib",
                "--emit=metadata",
                source.string(),
                "-o",
                corpus.rust_metadata_path(spec.workload).string(),
            };
    }
    return {};
}

[[nodiscard]] std::string command_line_for_message(const std::vector<std::string>& command)
{
    std::ostringstream out;
    for (std::size_t index = 0; index < command.size(); ++index) {
        if (index != 0) {
            out << ' ';
        }
        out << command[index];
    }
    return out.str();
}

[[nodiscard]] ProcessResult run_process(const std::vector<std::string>& command)
{
    if (command.empty()) {
        return {false, "empty benchmark command"};
    }

    const pid_t child = fork();
    if (child < 0) {
        return {false, "fork failed"};
    }
    if (child == 0) {
        const int null_fd = open(std::string(FRONTEND_COMPARE_DEV_NULL).c_str(), O_WRONLY);
        if (null_fd >= 0) {
            static_cast<void>(dup2(null_fd, STDOUT_FILENO));
            static_cast<void>(dup2(null_fd, STDERR_FILENO));
            close(null_fd);
        }

        std::vector<char*> argv;
        argv.reserve(command.size() + 1);
        for (const std::string& arg : command) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execv(argv[0], argv.data());
        _exit(errno == ENOENT ? FRONTEND_COMPARE_COMMAND_NOT_FOUND_EXIT : FRONTEND_COMPARE_COMMAND_NOT_EXECUTABLE_EXIT);
    }

    int status = 0;
    pid_t waited = 0;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited < 0) {
        return {false, "waitpid failed"};
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return {true, {}};
    }

    std::ostringstream message;
    message << "command failed";
    if (WIFEXITED(status)) {
        message << " with exit code " << WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        message << " from signal " << WTERMSIG(status);
    }
    message << ": " << command_line_for_message(command);
    return {false, message.str()};
}

[[nodiscard]] std::int64_t source_size_for(const std::filesystem::path& path)
{
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        return 0;
    }
    return static_cast<std::int64_t>(size);
}

void BM_FrontendCheck(benchmark::State& state, const BenchSpec spec)
{
    const SourceCorpus& corpus = source_corpus();
    const std::optional<std::filesystem::path> tool = find_tool(spec.compiler);
    if (!tool.has_value()) {
        std::string message =
            std::string("missing ") + compiler_label(spec.compiler) + " executable: " + requested_tool(spec.compiler);
        state.SkipWithError(message.c_str());
        return;
    }
    const std::filesystem::path source = corpus.source_path(spec.compiler, spec.workload);
    const std::int64_t source_bytes = source_size_for(source);
    const std::vector<std::string> command = build_command(spec, corpus, tool.value());

    for (auto _ : state) {
        const ProcessResult result = run_process(command);
        if (!result.ok) {
            state.SkipWithError(result.message.c_str());
            break;
        }
    }

    state.SetBytesProcessed(state.iterations() * source_bytes);
    state.SetItemsProcessed(state.iterations() * spec.item_count);
    state.counters["source_bytes"] = benchmark::Counter(static_cast<double>(source_bytes));
    state.counters["workload_items"] = benchmark::Counter(static_cast<double>(spec.item_count));
}

constexpr BenchSpec AUREX_LOOKUP_SPEC{
    FrontendCompiler::aurex,
    FrontendWorkload::lookup,
    FRONTEND_COMPARE_ITEM_COUNT,
};
constexpr BenchSpec AUREX_GENERICS_SPEC{
    FrontendCompiler::aurex,
    FrontendWorkload::generics,
    FRONTEND_COMPARE_ITEM_COUNT,
};
constexpr BenchSpec CLANGXX_LOOKUP_SPEC{
    FrontendCompiler::clangxx,
    FrontendWorkload::lookup,
    FRONTEND_COMPARE_ITEM_COUNT,
};
constexpr BenchSpec CLANGXX_GENERICS_SPEC{
    FrontendCompiler::clangxx,
    FrontendWorkload::generics,
    FRONTEND_COMPARE_ITEM_COUNT,
};
constexpr BenchSpec GXX_LOOKUP_SPEC{
    FrontendCompiler::gxx,
    FrontendWorkload::lookup,
    FRONTEND_COMPARE_ITEM_COUNT,
};
constexpr BenchSpec GXX_GENERICS_SPEC{
    FrontendCompiler::gxx,
    FrontendWorkload::generics,
    FRONTEND_COMPARE_ITEM_COUNT,
};
constexpr BenchSpec RUSTC_LOOKUP_SPEC{
    FrontendCompiler::rustc,
    FrontendWorkload::lookup,
    FRONTEND_COMPARE_ITEM_COUNT,
};
constexpr BenchSpec RUSTC_GENERICS_SPEC{
    FrontendCompiler::rustc,
    FrontendWorkload::generics,
    FRONTEND_COMPARE_ITEM_COUNT,
};

BENCHMARK_CAPTURE(BM_FrontendCheck, aurex_lookup_96, AUREX_LOOKUP_SPEC)->UseRealTime();
BENCHMARK_CAPTURE(BM_FrontendCheck, aurex_generics_96, AUREX_GENERICS_SPEC)->UseRealTime();
BENCHMARK_CAPTURE(BM_FrontendCheck, clangxx_lookup_96, CLANGXX_LOOKUP_SPEC)->UseRealTime();
BENCHMARK_CAPTURE(BM_FrontendCheck, clangxx_generics_96, CLANGXX_GENERICS_SPEC)->UseRealTime();
BENCHMARK_CAPTURE(BM_FrontendCheck, gxx_lookup_96, GXX_LOOKUP_SPEC)->UseRealTime();
BENCHMARK_CAPTURE(BM_FrontendCheck, gxx_generics_96, GXX_GENERICS_SPEC)->UseRealTime();
BENCHMARK_CAPTURE(BM_FrontendCheck, rustc_lookup_96, RUSTC_LOOKUP_SPEC)->UseRealTime();
BENCHMARK_CAPTURE(BM_FrontendCheck, rustc_generics_96, RUSTC_GENERICS_SPEC)->UseRealTime();

} // namespace

BENCHMARK_MAIN();
