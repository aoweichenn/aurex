#include "aurex/base/diagnostic.hpp"
#include "aurex/base/integer.hpp"
#include "aurex/lex/lexer.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

namespace {

constexpr aurex::base::usize default_iterations = 1000;
constexpr aurex::base::usize default_repetitions = 256;
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
    "ptr_cast bit_cast size_of align_of ptr_addr ptr_from_addr str_data str_byte_len "
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

[[nodiscard]] std::string make_source(
    const aurex::base::usize repetitions,
    const BenchmarkScenario scenario
) {
    const std::string_view snippet = scenario_snippet(scenario);
    std::string source;
    source.reserve(snippet.size() * repetitions);
    for (aurex::base::usize index = 0; index < repetitions; ++index) {
        source += snippet;
    }
    return source;
}

} // namespace

int main(const int argc, char** argv) {
    const aurex::base::usize iterations = argc > 1
        ? parse_positive_usize(argv[1], default_iterations)
        : default_iterations;
    const aurex::base::usize repetitions = argc > 2
        ? parse_positive_usize(argv[2], default_repetitions)
        : default_repetitions;
    const BenchmarkScenario scenario = argc > 3
        ? parse_scenario(argv[3])
        : BenchmarkScenario::mixed;
    const std::string source = make_source(repetitions, scenario);

    aurex::base::usize total_tokens = 0;
    const auto started = std::chrono::steady_clock::now();
    for (aurex::base::usize iteration = 0; iteration < iterations; ++iteration) {
        aurex::base::DiagnosticSink diagnostics;
        aurex::lex::Lexer lexer({1}, source, diagnostics);
        auto result = lexer.tokenize();
        if (!result || diagnostics.has_error()) {
            std::cerr << "lexer benchmark input failed to tokenize\n";
            return 1;
        }
        total_tokens += result.value().size();
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    const auto total_bytes = static_cast<double>(source.size()) * static_cast<double>(iterations);
    const auto total_token_count = static_cast<double>(total_tokens);
    const double elapsed_ms = static_cast<double>(elapsed_ns) / nanoseconds_per_millisecond;
    const double ns_per_byte = static_cast<double>(elapsed_ns) / total_bytes;
    const double ns_per_token = static_cast<double>(elapsed_ns) / total_token_count;

    std::cout << "scenario: " << scenario_name(scenario) << '\n'
              << "iterations: " << iterations << '\n'
              << "source_bytes: " << source.size() << '\n'
              << "total_bytes: " << static_cast<unsigned long long>(total_bytes) << '\n'
              << "total_tokens: " << total_tokens << '\n'
              << "elapsed_ms: " << elapsed_ms << '\n'
              << "ns_per_byte: " << ns_per_byte << '\n'
              << "ns_per_token: " << ns_per_token << '\n';
    return 0;
}
