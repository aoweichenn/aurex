#include <aurex/base/diagnostic.hpp>
#include <aurex/base/integer.hpp>
#include <aurex/base/source.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/sema/sema.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace {

constexpr aurex::base::SourceId FRONTEND_BENCH_SOURCE_ID {1};
constexpr aurex::base::usize LOOKUP_SOURCE_BYTES_PER_ITEM_ESTIMATE = 360;
constexpr aurex::base::usize GENERIC_SOURCE_BYTES_PER_ITEM_ESTIMATE = 430;
constexpr std::int64_t LEX_MIXED_SMALL_REPETITIONS = 64;
constexpr std::int64_t LEX_MIXED_LARGE_REPETITIONS = 128;
constexpr std::int64_t SEMA_LOOKUP_SMALL_ITEMS = 96;
constexpr std::int64_t SEMA_LOOKUP_LARGE_ITEMS = 192;
constexpr std::int64_t SEMA_GENERICS_SMALL_ITEMS = 64;
constexpr std::int64_t SEMA_GENERICS_LARGE_ITEMS = 128;

constexpr std::string_view LEX_MIXED_SNIPPET =
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

struct FrontendSummary final {
    aurex::base::usize token_count = 0;
    aurex::base::usize item_count = 0;
    aurex::base::usize expr_count = 0;
    aurex::base::usize function_count = 0;
    aurex::base::usize generic_function_instance_count = 0;
};

struct FrontendRunResult final {
    bool ok = false;
    std::string error;
    FrontendSummary summary;
};

[[nodiscard]] std::string first_diagnostic_or(
    const aurex::base::DiagnosticSink& diagnostics,
    const std::string_view fallback
) {
    const auto all = diagnostics.diagnostics();
    if (all.empty()) {
        return std::string(fallback);
    }
    return std::string(fallback) + ": " + all.front().message;
}

[[nodiscard]] FrontendRunResult tokenize_source(const std::string_view source) {
    aurex::base::DiagnosticSink diagnostics;
    aurex::lex::Lexer lexer(FRONTEND_BENCH_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        return FrontendRunResult {
            false,
            first_diagnostic_or(diagnostics, "lexing failed"),
            {},
        };
    }
    return FrontendRunResult {
        true,
        {},
        FrontendSummary {
            tokens.value().size(),
            0,
            0,
            0,
            0,
        },
    };
}

[[nodiscard]] FrontendRunResult analyze_source(const std::string_view source) {
    aurex::base::DiagnosticSink diagnostics;
    aurex::lex::Lexer lexer(FRONTEND_BENCH_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        return FrontendRunResult {
            false,
            first_diagnostic_or(diagnostics, "lexing failed"),
            {},
        };
    }

    aurex::parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        return FrontendRunResult {
            false,
            first_diagnostic_or(diagnostics, "parsing failed"),
            {},
        };
    }

    FrontendSummary summary;
    summary.token_count = tokens.value().size();
    summary.item_count = parsed.value().items.size();
    summary.expr_count = parsed.value().exprs.size();

    aurex::sema::SemanticOptions options;
    options.retain_generic_side_tables = false;
    aurex::sema::SemanticAnalyzer analyzer(parsed.take_value(), diagnostics, options);
    auto checked = analyzer.analyze();
    if (!checked) {
        return FrontendRunResult {
            false,
            first_diagnostic_or(diagnostics, "semantic analysis failed"),
            {},
        };
    }

    summary.function_count = checked.value().functions.size();
    summary.generic_function_instance_count = checked.value().generic_function_instances.size();
    if (summary.generic_function_instance_count == 0) {
        for (const auto& entry : checked.value().functions) {
            if (!entry.second.generic_args.empty()) {
                summary.generic_function_instance_count += 1;
            }
        }
    }
    return FrontendRunResult {
        true,
        {},
        summary,
    };
}

void append_repeated_snippet(
    std::string& source,
    const std::string_view snippet,
    const aurex::base::usize repetitions
) {
    for (aurex::base::usize index = 0; index < repetitions; ++index) {
        source += snippet;
    }
}

void append_lookup_record(std::string& source, const aurex::base::usize index) {
    const std::string suffix = std::to_string(index);
    source += "struct Rec";
    source += suffix;
    source += " {\n"
              "    left: i32;\n"
              "    right: i32;\n"
              "}\n\n";
}

void append_lookup_function(std::string& source, const aurex::base::usize index) {
    const std::string suffix = std::to_string(index);
    source += "fn helper";
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

[[nodiscard]] std::string make_lookup_source(const aurex::base::usize item_count) {
    std::string source;
    source.reserve(LOOKUP_SOURCE_BYTES_PER_ITEM_ESTIMATE * item_count);
    source += "module bench.lookup;\n\n";
    for (aurex::base::usize index = 0; index < item_count; ++index) {
        append_lookup_record(source, index);
    }
    for (aurex::base::usize index = 0; index < item_count; ++index) {
        append_lookup_function(source, index);
    }
    source += "fn main() -> i32 {\n"
              "    var total: i32 = 0;\n";
    for (aurex::base::usize index = 0; index < item_count; ++index) {
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

void append_payload_record(std::string& source, const aurex::base::usize index) {
    const std::string suffix = std::to_string(index);
    source += "struct Payload";
    source += suffix;
    source += " {\n"
              "    value: i32;\n"
              "}\n\n";
}

void append_payload_use_function(std::string& source, const aurex::base::usize index) {
    const std::string suffix = std::to_string(index);
    source += "fn use_payload";
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

[[nodiscard]] std::string make_generic_source(const aurex::base::usize item_count) {
    std::string source;
    source.reserve(GENERIC_SOURCE_BYTES_PER_ITEM_ESTIMATE * item_count);
    source +=
        "module bench.generics;\n\n"
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
    for (aurex::base::usize index = 0; index < item_count; ++index) {
        append_payload_record(source, index);
    }
    for (aurex::base::usize index = 0; index < item_count; ++index) {
        append_payload_use_function(source, index);
    }
    source += "fn main() -> i32 {\n"
              "    var total: i32 = 0;\n";
    for (aurex::base::usize index = 0; index < item_count; ++index) {
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

[[nodiscard]] std::string make_lex_source(const aurex::base::usize repetitions) {
    std::string source;
    source.reserve(LEX_MIXED_SNIPPET.size() * repetitions);
    append_repeated_snippet(source, LEX_MIXED_SNIPPET, repetitions);
    return source;
}

[[nodiscard]] std::int64_t processed_count(
    const std::int64_t iterations,
    const aurex::base::usize count
) noexcept {
    return iterations * static_cast<std::int64_t>(count);
}

void set_frontend_counters(
    benchmark::State& state,
    const std::string& source,
    const FrontendSummary& summary
) {
    state.counters["source_bytes"] = benchmark::Counter(static_cast<double>(source.size()));
    state.counters["tokens"] = benchmark::Counter(static_cast<double>(summary.token_count));
    state.counters["ast_items"] = benchmark::Counter(static_cast<double>(summary.item_count));
    state.counters["ast_exprs"] = benchmark::Counter(static_cast<double>(summary.expr_count));
    state.counters["functions"] = benchmark::Counter(static_cast<double>(summary.function_count));
    state.counters["generic_function_instances"] = benchmark::Counter(
        static_cast<double>(summary.generic_function_instance_count)
    );
}

void BM_LexMixed(benchmark::State& state) {
    const auto repetitions = static_cast<aurex::base::usize>(state.range(0));
    const std::string source = make_lex_source(repetitions);
    FrontendSummary summary;

    for (auto _ : state) {
        auto result = tokenize_source(source);
        if (!result.ok) {
            state.SkipWithError(result.error.c_str());
            break;
        }
        summary = result.summary;
        benchmark::DoNotOptimize(summary.token_count);
    }

    state.SetBytesProcessed(processed_count(state.iterations(), source.size()));
    state.SetItemsProcessed(processed_count(state.iterations(), summary.token_count));
    set_frontend_counters(state, source, summary);
}

void BM_SemaLookup(benchmark::State& state) {
    const auto item_count = static_cast<aurex::base::usize>(state.range(0));
    const std::string source = make_lookup_source(item_count);
    FrontendSummary summary;

    for (auto _ : state) {
        auto result = analyze_source(source);
        if (!result.ok) {
            state.SkipWithError(result.error.c_str());
            break;
        }
        summary = result.summary;
        benchmark::DoNotOptimize(summary.function_count);
    }

    state.SetBytesProcessed(processed_count(state.iterations(), source.size()));
    state.SetItemsProcessed(processed_count(state.iterations(), item_count));
    set_frontend_counters(state, source, summary);
}

void BM_SemaGenerics(benchmark::State& state) {
    const auto item_count = static_cast<aurex::base::usize>(state.range(0));
    const std::string source = make_generic_source(item_count);
    FrontendSummary summary;

    for (auto _ : state) {
        auto result = analyze_source(source);
        if (!result.ok) {
            state.SkipWithError(result.error.c_str());
            break;
        }
        summary = result.summary;
        benchmark::DoNotOptimize(summary.generic_function_instance_count);
    }

    state.SetBytesProcessed(processed_count(state.iterations(), source.size()));
    state.SetItemsProcessed(processed_count(state.iterations(), item_count));
    set_frontend_counters(state, source, summary);
}

BENCHMARK(BM_LexMixed)->Arg(LEX_MIXED_SMALL_REPETITIONS)->Arg(LEX_MIXED_LARGE_REPETITIONS);
BENCHMARK(BM_SemaLookup)->Arg(SEMA_LOOKUP_SMALL_ITEMS)->Arg(SEMA_LOOKUP_LARGE_ITEMS);
BENCHMARK(BM_SemaGenerics)->Arg(SEMA_GENERICS_SMALL_ITEMS)->Arg(SEMA_GENERICS_LARGE_ITEMS);

} // namespace

BENCHMARK_MAIN();
