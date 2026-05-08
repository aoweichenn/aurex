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

constexpr std::string_view lexer_benchmark_snippet =
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

[[nodiscard]] std::string make_source(const aurex::base::usize repetitions) {
    std::string source;
    source.reserve(lexer_benchmark_snippet.size() * repetitions);
    for (aurex::base::usize index = 0; index < repetitions; ++index) {
        source += lexer_benchmark_snippet;
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
    const std::string source = make_source(repetitions);

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
    const double elapsed_ms = static_cast<double>(elapsed_ns) / nanoseconds_per_millisecond;
    const double ns_per_byte = static_cast<double>(elapsed_ns) / total_bytes;

    std::cout << "iterations: " << iterations << '\n'
              << "source_bytes: " << source.size() << '\n'
              << "total_bytes: " << static_cast<unsigned long long>(total_bytes) << '\n'
              << "total_tokens: " << total_tokens << '\n'
              << "elapsed_ms: " << elapsed_ms << '\n'
              << "ns_per_byte: " << ns_per_byte << '\n';
    return 0;
}
