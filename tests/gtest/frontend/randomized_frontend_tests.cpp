#include <aurex/base/diagnostic.hpp>
#include <aurex/ir/ir_dump.hpp>
#include <aurex/ir/lower_ast.hpp>
#include <aurex/ir/pass_pipeline.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/sema/sema.hpp>
#include <aurex/syntax/ast_dump.hpp>

#include <support/randomized_source.hpp>

#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using base::DiagnosticSink;

constexpr base::SourceId RANDOMIZED_FRONTEND_LEXER_SOURCE_ID{1};
constexpr base::SourceId RANDOMIZED_FRONTEND_PARSER_SOURCE_ID{2};
constexpr base::SourceId RANDOMIZED_FRONTEND_PIPELINE_SOURCE_ID{3};

void exercise_lexer_only(const std::string_view source)
{
    DiagnosticSink diagnostics;
    lex::Lexer lexer(RANDOMIZED_FRONTEND_LEXER_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (tokens) {
        EXPECT_FALSE(tokens.value().empty());
        static_cast<void>(syntax::dump_tokens(tokens.value()));
    } else {
        EXPECT_TRUE(diagnostics.has_error());
    }
}

void exercise_parser_recovery(const std::string_view source)
{
    DiagnosticSink diagnostics;
    lex::Lexer lexer(RANDOMIZED_FRONTEND_PARSER_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        EXPECT_TRUE(diagnostics.has_error());
        return;
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (parsed) {
        static_cast<void>(syntax::dump_ast(parsed.value()));
    } else {
        EXPECT_TRUE(diagnostics.has_error());
    }
}

void exercise_module_pipeline(const std::string_view source)
{
    DiagnosticSink diagnostics;
    lex::Lexer lexer(RANDOMIZED_FRONTEND_PIPELINE_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message << "\n" << source;
    EXPECT_FALSE(tokens.value().empty());
    static_cast<void>(syntax::dump_tokens(tokens.value()));

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message << " @ " << diagnostic.range.begin << ".." << diagnostic.range.end;
        }
    }
    ASSERT_TRUE(parsed) << parsed.error().message << "\n" << source;
    ASSERT_FALSE(diagnostics.has_error()) << source;
    static_cast<void>(syntax::dump_ast(parsed.value()));

    sema::SemanticAnalyzer analyzer(parsed.value(), diagnostics);
    auto checked = analyzer.analyze();
    ASSERT_TRUE(checked) << checked.error().message << "\n" << source;
    static_cast<void>(sema::dump_checked_module(checked.value()));

    auto lowered = ir::lower_ast(parsed.value(), checked.value());
    ASSERT_TRUE(lowered) << lowered.error().message << "\n" << source;
    auto optimized = ir::run_pass_pipeline(lowered.value(),
        ir::PassPipelineOptions{
            ir::OptimizationLevel::basic,
            true,
            true,
            true,
            true,
        });
    ASSERT_TRUE(optimized) << optimized.error().message << "\n" << source;
    static_cast<void>(ir::dump_module(lowered.value()));
}

} // namespace

TEST(CoreUnit, RandomizedLexerHandlesMixedNoiseDeterministically)
{
    randomized::DeterministicRandom random(randomized::RANDOM_SOURCE_LEXER_NOISE_SEED);
    for (base::usize index = 0; index < randomized::RANDOM_SOURCE_DEFAULT_LEXER_NOISE_COUNT; ++index) {
        SCOPED_TRACE("random lexer noise case " + std::to_string(index));
        exercise_lexer_only(randomized::lexer_noise_source(random, index));
    }
}

TEST(CoreUnit, RandomizedParserRecoversFromMixedInvalidSourcesDeterministically)
{
    randomized::DeterministicRandom random(randomized::RANDOM_SOURCE_PARSER_RECOVERY_SEED);
    for (base::usize index = 0; index < randomized::RANDOM_SOURCE_DEFAULT_PARSER_RECOVERY_COUNT; ++index) {
        SCOPED_TRACE("random parser recovery case " + std::to_string(index));
        exercise_parser_recovery(randomized::parser_recovery_source(random, index));
    }
}

TEST(CoreUnit, RandomizedFrontendPipelineAcceptsMixedValidModulesDeterministically)
{
    randomized::DeterministicRandom random(randomized::RANDOM_SOURCE_LEGAL_PROGRAM_SEED);
    for (base::usize index = 0; index < randomized::RANDOM_SOURCE_DEFAULT_LEGAL_PROGRAM_COUNT; ++index) {
        SCOPED_TRACE("random valid module case " + std::to_string(index));
        exercise_module_pipeline(randomized::legal_program(random, index));
    }
}

} // namespace aurex::test
