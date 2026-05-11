#include <aurex/base/diagnostic.hpp>
#include <aurex/base/integer.hpp>
#include <aurex/lex/lexer.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr aurex::base::usize default_iterations = 1000;
constexpr aurex::base::usize default_repetitions = 256;
constexpr aurex::base::usize default_warmup_iterations = 0;
constexpr aurex::base::usize default_runs = 1;
constexpr aurex::base::usize first_position_arg = 1;
constexpr aurex::base::usize max_position_args = 3;
constexpr double nanoseconds_per_millisecond = 1'000'000.0;

enum class BenchmarkScenario {
    mixed,
    identifiers,
    numbers,
    strings,
    punctuation,
};

constexpr std::string_view mixed_benchmark_snippet =
    "module bench.lex;\n"
    "const hex: i32 = 0x2A;\n"
    "const bin: i32 = 0b1010;\n"
    "const dec: i32 = 1_000;\n"
    "const flt: f64 = 1.25e+2;\n"
    "const s: str = \"hello\\\\n\";\n"
    "const c: *const u8 = c\"hello\";\n"
    "const b: u8 = b'\\n';\n"
    "fn ops(a: i32, b: i32) -> i32 { return ((a / b) % 3) ^ (a << 1) >> 1 | ~b; }\n"
    "fn flags(flag: bool) -> void { if true && !false || flag { return; } }\n"
    "... . :: : -> - => == = != ! <= << < >= >> > && & || | ( ) { } [ ] , ; + * / % ^ ~ @ ?\n";

constexpr std::string_view identifier_benchmark_snippet =
    "module identifiers.bench;\n"
    "fn names(alpha_value: i32, beta_value: i32, gamma_value: i32) -> i32 {\n"
    "  let local_counter: i32 = alpha_value + beta_value + gamma_value;\n"
    "  var mutable_state: i32 = local_counter;\n"
    "  if true { return mutable_state; } else { return alpha_value; }\n"
    "}\n"
    "module import as pub priv extern export c fn struct opaque enum const type impl match "
    "let var if else for while break continue defer return noncopy move true false null "
    "void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str mut cast "
    "ptr_cast bcast size_of align_of ptr_addr paddr str_data str_byte_len "
    "str_from_bytes_unchecked\n";

constexpr std::string_view number_benchmark_snippet =
    "0 1 12 123 1_000 123_456_789 0x2A 0XFF 0xCAFE_BABE "
    "0b1010 0B1111_0000 1.0 12.34 1e10 1e+10 1.25e-2\n";

constexpr std::string_view string_benchmark_snippet =
    "\"plain ascii\" \"escaped\\n\\t\\\"\" \"unicode \\u{03A9}\" "
    "c\"plain\" c\"escaped\\n\" b'a' b'\\n'\n";

constexpr std::string_view punctuation_benchmark_snippet =
    "... . :: : -> - => == = != ! <= << < >= >> > && & || | "
    "( ) { } [ ] , ; + * / % ^ ~ @ ?\n";

struct BenchmarkConfig final {
    aurex::base::usize iterations = default_iterations;
    aurex::base::usize repetitions = default_repetitions;
    aurex::base::usize warmup_iterations = default_warmup_iterations;
    aurex::base::usize runs = default_runs;
    BenchmarkScenario scenario = BenchmarkScenario::mixed;
    std::string file_path;
};

struct RunMeasurement final {
    aurex::base::usize elapsed_ns = 0;
    aurex::base::usize total_tokens = 0;
};

struct MetricSummary final {
    double min = 0.0;
    double median = 0.0;
    double max = 0.0;
    double average = 0.0;
};

[[nodiscard]] BenchmarkScenario parse_scenario(const std::string_view text) noexcept {
    if (text == "identifiers") {
        return BenchmarkScenario::identifiers;
    }
    if (text == "numbers") {
        return BenchmarkScenario::numbers;
    }
    if (text == "strings") {
        return BenchmarkScenario::strings;
    }
    if (text == "punctuation") {
        return BenchmarkScenario::punctuation;
    }
    return BenchmarkScenario::mixed;
}

[[nodiscard]] bool is_option(const std::string_view text) noexcept {
    return text.starts_with("--");
}

[[nodiscard]] bool option_requires_value(const std::string_view text) noexcept {
    return text == "--file" || text == "--warmup" || text == "--runs";
}

[[nodiscard]] std::string_view scenario_name(const BenchmarkScenario scenario) noexcept {
    switch (scenario) {
    case BenchmarkScenario::mixed:
        return "mixed";
    case BenchmarkScenario::identifiers:
        return "identifiers";
    case BenchmarkScenario::numbers:
        return "numbers";
    case BenchmarkScenario::strings:
        return "strings";
    case BenchmarkScenario::punctuation:
        return "punctuation";
    }
    return "mixed";
}

[[nodiscard]] std::string_view scenario_snippet(const BenchmarkScenario scenario) noexcept {
    switch (scenario) {
    case BenchmarkScenario::mixed:
        return mixed_benchmark_snippet;
    case BenchmarkScenario::identifiers:
        return identifier_benchmark_snippet;
    case BenchmarkScenario::numbers:
        return number_benchmark_snippet;
    case BenchmarkScenario::strings:
        return string_benchmark_snippet;
    case BenchmarkScenario::punctuation:
        return punctuation_benchmark_snippet;
    }
    return mixed_benchmark_snippet;
}

[[nodiscard]] std::string input_name(const BenchmarkConfig& config) {
    if (!config.file_path.empty()) {
        return config.file_path;
    }
    return std::string(scenario_name(config.scenario));
}

[[nodiscard]] aurex::base::usize parse_positive_usize(
    const char* const text,
    const aurex::base::usize fallback
) {
    try {
        const auto parsed = static_cast<aurex::base::usize>(std::stoull(text));
        return parsed == 0 ? fallback : parsed;
    } catch (...) {
        return fallback;
    }
}

void apply_position_arg(BenchmarkConfig& config, const aurex::base::usize position, const char* const text) {
    switch (position) {
    case 0:
        config.iterations = parse_positive_usize(text, default_iterations);
        return;
    case 1:
        config.repetitions = parse_positive_usize(text, default_repetitions);
        return;
    case 2:
        config.scenario = parse_scenario(text);
        return;
    default:
        return;
    }
}

bool apply_option(
    BenchmarkConfig& config,
    const std::string_view option,
    const char* const value
) {
    if (option == "--file") {
        config.file_path = value;
        return true;
    }
    if (option == "--warmup") {
        config.warmup_iterations = parse_positive_usize(value, default_warmup_iterations);
        return true;
    }
    if (option == "--runs") {
        config.runs = parse_positive_usize(value, default_runs);
        return true;
    }
    return false;
}

[[nodiscard]] BenchmarkConfig parse_config(const int argc, char** argv) {
    BenchmarkConfig config;
    aurex::base::usize position = 0;
    for (int index = first_position_arg; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (!is_option(arg)) {
            if (position < max_position_args) {
                apply_position_arg(config, position, argv[index]);
            }
            ++position;
            continue;
        }

        if (!option_requires_value(arg) || index + 1 >= argc) {
            std::cerr << "unknown or incomplete option: " << arg << '\n';
            continue;
        }
        if (!apply_option(config, arg, argv[index + 1])) {
            std::cerr << "unknown option: " << arg << '\n';
        }
        ++index;
    }
    return config;
}

[[nodiscard]] std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

[[nodiscard]] std::string make_source(
    const aurex::base::usize repetitions,
    const std::string_view snippet
) {
    std::string source;
    source.reserve(snippet.size() * repetitions);
    for (aurex::base::usize index = 0; index < repetitions; ++index) {
        source += snippet;
    }
    return source;
}

[[nodiscard]] std::string make_source(const BenchmarkConfig& config) {
    if (!config.file_path.empty()) {
        const std::string file_text = read_file(config.file_path);
        return make_source(config.repetitions, file_text);
    }
    return make_source(config.repetitions, scenario_snippet(config.scenario));
}

[[nodiscard]] bool tokenize_source(
    const std::string& source,
    const aurex::base::usize iterations,
    aurex::base::usize& total_tokens
) {
    total_tokens = 0;
    for (aurex::base::usize iteration = 0; iteration < iterations; ++iteration) {
        aurex::base::DiagnosticSink diagnostics;
        aurex::lex::Lexer lexer({1}, source, diagnostics);
        auto result = lexer.tokenize();
        if (!result || diagnostics.has_error()) {
            return false;
        }
        total_tokens += result.value().size();
    }
    return true;
}

[[nodiscard]] RunMeasurement measure_run(
    const std::string& source,
    const aurex::base::usize iterations
) {
    aurex::base::usize total_tokens = 0;
    const auto started = std::chrono::steady_clock::now();
    if (!tokenize_source(source, iterations, total_tokens)) {
        return {};
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    return RunMeasurement {
        static_cast<aurex::base::usize>(elapsed_ns),
        total_tokens,
    };
}

[[nodiscard]] MetricSummary summarize(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    const aurex::base::usize median_index = values.size() / 2;
    return MetricSummary {
        values.front(),
        values[median_index],
        values.back(),
        total / static_cast<double>(values.size()),
    };
}

void print_summary(const std::string_view label, const MetricSummary summary) {
    std::cout << label << "_min: " << summary.min << '\n'
              << label << "_median: " << summary.median << '\n'
              << label << "_max: " << summary.max << '\n'
              << label << "_avg: " << summary.average << '\n';
}

} // namespace

int main(const int argc, char** argv) {
    const BenchmarkConfig config = parse_config(argc, argv);
    const std::string source = make_source(config);
    if (source.empty()) {
        std::cerr << "lexer benchmark input is empty or unreadable\n";
        return 1;
    }

    if (config.warmup_iterations > 0) {
        aurex::base::usize warmup_tokens = 0;
        if (!tokenize_source(source, config.warmup_iterations, warmup_tokens)) {
            std::cerr << "lexer benchmark warmup input failed to tokenize\n";
            return 1;
        }
    }

    std::vector<double> elapsed_ms_values;
    std::vector<double> ns_per_byte_values;
    std::vector<double> ns_per_token_values;
    elapsed_ms_values.reserve(config.runs);
    ns_per_byte_values.reserve(config.runs);
    ns_per_token_values.reserve(config.runs);

    aurex::base::usize last_total_tokens = 0;
    for (aurex::base::usize run = 0; run < config.runs; ++run) {
        const RunMeasurement measurement = measure_run(source, config.iterations);
        if (measurement.elapsed_ns == 0 || measurement.total_tokens == 0) {
            std::cerr << "lexer benchmark input failed to tokenize\n";
            return 1;
        }
        const auto total_bytes = static_cast<double>(source.size()) * static_cast<double>(config.iterations);
        const auto total_token_count = static_cast<double>(measurement.total_tokens);
        elapsed_ms_values.push_back(static_cast<double>(measurement.elapsed_ns) / nanoseconds_per_millisecond);
        ns_per_byte_values.push_back(static_cast<double>(measurement.elapsed_ns) / total_bytes);
        ns_per_token_values.push_back(static_cast<double>(measurement.elapsed_ns) / total_token_count);
        last_total_tokens = measurement.total_tokens;
    }

    std::cout << std::setprecision(6)
              << "input: " << input_name(config) << '\n'
              << "iterations: " << config.iterations << '\n'
              << "repetitions: " << config.repetitions << '\n'
              << "warmup_iterations: " << config.warmup_iterations << '\n'
              << "runs: " << config.runs << '\n'
              << "source_bytes: " << source.size() << '\n'
              << "total_bytes_per_run: "
              << static_cast<unsigned long long>(static_cast<double>(source.size()) * static_cast<double>(config.iterations))
              << '\n'
              << "total_tokens_per_run: " << last_total_tokens << '\n';
    print_summary("elapsed_ms", summarize(elapsed_ms_values));
    print_summary("ns_per_byte", summarize(ns_per_byte_values));
    print_summary("ns_per_token", summarize(ns_per_token_values));
    return 0;
}
