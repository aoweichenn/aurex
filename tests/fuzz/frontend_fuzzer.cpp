#include <aurex/base/diagnostic.hpp>
#include <aurex/ir/lower_ast.hpp>
#include <aurex/ir/pass_pipeline.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/sema/sema.hpp>
#include <aurex/syntax/ast_dump.hpp>

#include <support/randomized_source.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace aurex::test {
namespace {

constexpr base::SourceId FRONTEND_FUZZER_SOURCE_ID{1};

void exercise_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(FRONTEND_FUZZER_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        return;
    }
    static_cast<void>(syntax::dump_tokens(tokens.value()));

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        return;
    }
    static_cast<void>(syntax::dump_ast(parsed.value()));

    sema::SemanticAnalyzer analyzer(parsed.value(), diagnostics);
    auto checked = analyzer.analyze();
    if (!checked) {
        return;
    }

    auto lowered = ir::lower_ast(parsed.value(), checked.value());
    if (!lowered) {
        return;
    }
    static_cast<void>(ir::run_pass_pipeline(lowered.value(),
        ir::PassPipelineOptions{
            ir::OptimizationLevel::basic,
            true,
            true,
            true,
            true,
        }));
}

[[nodiscard]] std::string generated_source_from_seed(const std::uint64_t seed)
{
    randomized::DeterministicRandom random(seed);
    switch (random.index(randomized::RANDOM_SOURCE_FUZZ_GENERATOR_KIND_COUNT)) {
        case 0:
            return randomized::legal_program(random, random.index(randomized::RANDOM_SOURCE_IDENTIFIER_SPAN));
        case 1:
            return randomized::parser_recovery_source(random, random.index(randomized::RANDOM_SOURCE_IDENTIFIER_SPAN));
        default:
            return randomized::lexer_noise_source(random, random.index(randomized::RANDOM_SOURCE_IDENTIFIER_SPAN));
    }
}

[[nodiscard]] std::uint64_t seed_from_bytes(const std::uint8_t* data, const std::size_t size) noexcept
{
    std::uint64_t seed = randomized::RANDOM_SOURCE_LEGAL_PROGRAM_SEED;
    for (std::size_t index = 0; index < size; ++index) {
        seed ^= static_cast<std::uint64_t>(data[index])
            << ((index % sizeof(std::uint64_t)) * randomized::RANDOM_SOURCE_BITS_PER_BYTE);
        seed *= randomized::RANDOM_SOURCE_SPLITMIX_FIRST_MULTIPLIER;
    }
    return seed;
}

} // namespace
} // namespace aurex::test

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    const std::string_view raw_source(reinterpret_cast<const char*>(data), size);
    aurex::test::exercise_source(raw_source);

    const std::string generated = aurex::test::generated_source_from_seed(aurex::test::seed_from_bytes(data, size));
    aurex::test::exercise_source(generated);
    return 0;
}
